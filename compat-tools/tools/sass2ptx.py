#!/usr/bin/env python3
"""
sass2ptx.py - translate the Fermi (sm_20) SASS inside Brigade 2's
TracerCUDA_sm20.cubin into 32-bit PTX that a modern driver can JIT.

Pipeline:  cubin --(ELF parse)--> .text.<kernel> --(envydis -m gf100)--> text
           --(this script)--> PTX (.address_size 32, .target sm_50)

Design notes (all verified against the real disassembly):
  * every SASS register $rN becomes a PTX .b32 register %rN (PTX lets bit-size
    registers feed any 32-bit typed instruction); $pN -> .pred %pN.
  * SIMT-stack instructions (joinat / join / precont / prebrk / brk / cont,
    bra allwarp) are reconvergence machinery.  PTX has per-thread control
    flow, so joinat/join are dropped, brk/cont are resolved statically to
    plain branches (innermost enclosing prebrk / precont), allwarp is a hint.
  * c0[] = launch constants, c1[0x100] = local stack top, c2[] = __constant__
    `context`, c16[] = compiler literal pool (inlined as immediates).
  * `add rX rY c0[0x4]` is "local address -> generic address" (cvta.local).
  * explicit .rn on every float op so ptxas never contracts mul+add.
"""
import re, struct, subprocess, sys, os

DEBUG = False
CLOCK = None
TRACE = None   # (label addr, [regs], pred-regs) : thread 0 logs these at the label
DEBUG_LIMIT = 100000
HERE = os.path.dirname(os.path.abspath(__file__))
ENVYDIS = os.path.join(HERE, 'envydis.exe')


# --------------------------------------------------------------------------
# ELF helpers
# --------------------------------------------------------------------------
def read_cubin(path):
    d = open(path, 'rb').read()
    shoff = struct.unpack_from('<I', d, 0x20)[0]
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
    secs = []
    for i in range(shnum):
        nm, ty, fl, ad, off, sz, lk, inf, al, es = struct.unpack_from('<10I', d, shoff + i * shentsize)
        secs.append((nm, off, sz))
    stro = secs[shstrndx][1]
    out = {}
    for nm, off, sz in secs:
        e = d.index(b'\0', stro + nm)
        out[d[stro + nm:e].decode()] = d[off:off + sz]
    return out


def disassemble(code):
    tmp = os.path.join(os.environ.get('TEMP', '.'), '_s2p.bin')
    open(tmp, 'wb').write(code)
    r = subprocess.run([ENVYDIS, '-m', 'gf100', '-n', '-i', tmp], capture_output=True, text=True)
    return r.stdout.splitlines()


# --------------------------------------------------------------------------
# parsing
# --------------------------------------------------------------------------
LINE = re.compile(r'^([0-9a-f]{8}): ([0-9a-f]{8}) ([0-9a-f]{8})\s+(.*)$')


def tokenize(s):
    return re.findall(r'\(|\)|[^\s()]+', s)


class Insn:
    pass


def parse_lines(lines):
    out = []
    for ln in lines:
        m = LINE.match(ln.rstrip())
        if not m:
            raise ValueError('unparsable line: ' + ln)
        i = Insn()
        i.addr = int(m.group(1), 16)
        rest = m.group(4)
        toks = tokenize(rest)
        i.target_mark = False
        if toks and toks[0] == 'B':
            i.target_mark = True
            toks = toks[1:]
        i.join = False
        if toks and toks[0] == 'join':
            i.join = True
            toks = toks[1:]
        i.guard = None  # (reg, negated)
        if toks and re.fullmatch(r'\$p\d', toks[0]):
            i.guard = (toks[0][1:], False)
            toks = toks[1:]
        elif len(toks) >= 4 and toks[0] == '(' and toks[1] == 'not' and re.fullmatch(r'\$p\d', toks[2]) and toks[3] == ')':
            i.guard = (toks[2][1:], True)
            toks = toks[4:]
        i.toks = toks
        i.text = rest
        out.append(i)
    return out


# --------------------------------------------------------------------------
# translation
# --------------------------------------------------------------------------
def f32imm(v):
    return '0f%08X' % (v & 0xffffffff)


class Xlat:
    def __init__(self, name, insns, const16):
        self.name = name
        self.insns = insns
        self.c16 = const16
        self.out = []
        self.ntmp = 0
        self.maxtmp = 0
        self.maxtp = 0
        self.ntp = 0
        self.stats = {}

    # ---- temps
    def tmp(self):
        t = '%%t%d' % self.ntmp
        self.ntmp += 1
        self.maxtmp = max(self.maxtmp, self.ntmp)
        return t

    def ptmp(self):
        t = '%%q%d' % self.ntp
        self.ntp += 1
        self.maxtp = max(self.maxtp, self.ntp)
        return t

    def emit(self, s):
        self.out.append('\t' + s)

    def emit_g(self, s, guard):
        """emit with the instruction guard applied"""
        if guard:
            reg, neg = guard
            s = '@%s%%%s %s' % ('!' if neg else '', reg, s)
        self.emit(s)

    # ---- operands
    def pred(self, tok, neg=False):
        m = re.fullmatch(r'\$p(\d)', tok)
        if m:
            return '%%p%s' % m.group(1)
        raise ValueError('bad pred ' + tok)

    def reg(self, tok):
        m = re.fullmatch(r'\$r(\d+)[dq]?', tok)
        if not m:
            raise ValueError('bad reg ' + tok)
        return '%%r%s' % m.group(1)

    def regn(self, tok):
        """(base index, count) for $rN / $rNd / $rNq"""
        m = re.fullmatch(r'\$r(\d+)([dq]?)', tok)
        n = int(m.group(1))
        return n, {'': 1, 'd': 2, 'q': 4}[m.group(2)]

    def src(self, toks, i, kind):
        """parse a source operand (with neg/abs/not prefixes). kind: 'f' or 'i'.
        returns (operand string, next index)"""
        neg = absf = nt = False
        while toks[i] in ('neg', 'abs', 'not'):
            if toks[i] == 'neg': neg = True
            elif toks[i] == 'abs': absf = True
            else: nt = True
            i += 1
        t = toks[i]
        i += 1
        o = self.base(t, kind)
        if absf:
            n = self.tmp()
            self.emit('abs.f32 %s, %s;' % (n, o)) if kind == 'f' else self.emit('abs.s32 %s, %s;' % (n, o))
            o = n
        if neg:
            n = self.tmp()
            self.emit('neg.f32 %s, %s;' % (n, o)) if kind == 'f' else self.emit('neg.s32 %s, %s;' % (n, o))
            o = n
        if nt:
            n = self.tmp()
            self.emit('not.b32 %s, %s;' % (n, o))
            o = n
        return o, i

    def base(self, t, kind):
        if t.startswith('$r'):
            return self.reg(t)
        if re.fullmatch(r'-?0x[0-9a-f]+', t):
            v = int(t, 16)
            if kind == 'f':
                return f32imm(v)
            return str(v)
        m = re.fullmatch(r'c(\d+)\[(.*)\]', t)
        if m:
            bank, idx = int(m.group(1)), m.group(2)
            if bank == 16:
                off = int(idx, 16)
                v = struct.unpack_from('<I', self.c16, off)[0]
                return f32imm(v) if kind == 'f' else str(v)
            if bank == 2:
                if re.fullmatch(r'0x[0-9a-f]+', idx):
                    n = self.tmp()
                    self.emit('ld.const.b32 %s, [context+%d];' % (n, int(idx, 16)))
                    return n
                # register indexed
                mm = re.fullmatch(r'\$r(\d+)', idx)
                a = self.tmp(); n = self.tmp()
                self.emit('mov.u32 %s, context;' % a)
                self.emit('add.u32 %s, %s, %%r%s;' % (a, a, mm.group(1)))
                self.emit('ld.const.b32 %s, [%s];' % (n, a))
                return n
            if bank == 0:
                off = int(idx, 16)
                sr = {0xc: '%ntid.y', 0x14: '%nctaid.x', 0x8: '%ntid.x', 0x10: '%ntid.z',
                      0x18: '%nctaid.y', 0x1c: '%nctaid.z'}[off]
                n = self.tmp()
                self.emit('mov.u32 %s, %s;' % (n, sr))
                return n
        raise ValueError('unhandled operand ' + t)

    def addr(self, t):
        """l[$r1+0x70] / g[$r13] -> ('l'|'g', '[%r1+112]')"""
        m = re.fullmatch(r'([lg])\[\$r(\d+)(?:([+-])(0x[0-9a-f]+))?\]', t)
        if not m:
            raise ValueError('bad mem ' + t)
        sp, r, sg, off = m.group(1), m.group(2), m.group(3), m.group(4)
        a = '[%%r%s%s]' % (r, (('+%d' % (int(off,16) * (-1 if sg=='-' else 1)))) if off else '')
        return sp, a

    # ---- main
    def run(self):
        insns = self.insns
        # collect structure
        prebrks = []   # (addr, target)
        precont = []
        targets = set()
        for ins in insns:
            t = ins.toks
            if not t:
                continue
            if t[0] == 'prebrk': prebrks.append((ins.addr, int(t[1], 16)))
            if t[0] == 'precont': precont.append((ins.addr, int(t[1], 16)))
            if t[0] == 'bra': targets.add(int(t[-1], 16))
        joinats = [(i.addr, int(i.toks[1], 16)) for i in insns if i.toks and i.toks[0] == 'joinat']
        self.jtarget = {}
        for ins in insns:
            if ins.join:
                cands = sorted([(x, -a) for a, x in joinats if a < ins.addr and x >= ins.addr + 8])
                if not cands:
                    raise ValueError('join without joinat at %x' % ins.addr)
                self.jtarget[ins.addr] = cands[0][0]
                if cands[0][0] != ins.addr + 8:
                    targets.add(cands[0][0])
        for _, tg in prebrks: targets.add(tg)
        for _, tg in precont: targets.add(tg)
        self.targets = targets

        def brk_target(a):
            c = [(pa, tg) for pa, tg in prebrks if pa < a < tg or (pa < a and tg > a)]
            if not c: raise ValueError('no prebrk for brk at %x' % a)
            return max(c)[1]

        def cont_target(a):
            c = [(pa, tg) for pa, tg in precont if pa < a]
            if not c: raise ValueError('no precont for cont at %x' % a)
            return max(c)[1]

        for ins in insns:
            self.ntmp = 0
            self.ntp = 0
            if ins.addr in targets:
                self.out.append('L_%x:' % ins.addr)
                if TRACE and ins.addr == TRACE[0]:
                    regs = TRACE[1]
                    self.out += ['	mov.u32 %dt, %tid.x;', '	mov.u32 %dt2, %tid.y;', '	or.b32 %dt, %dt, %dt2;',
                                 '	setp.ne.u32 %dtp, %dt, 0;', '	setp.ge.u32 %dtq, %dtc, 200;', '	or.pred %dtp, %dtp, %dtq;',
                                 '	@%dtp bra TRSKIP;',
                                 '	ld.const.b32 %dto, [context+544];', '	mul.lo.u32 %dt, %dtc, ' + str(16 * ((len(regs) + 3) // 4)) + ';',
                                 '	add.u32 %dto, %dto, %dt;']
                    for k, rname in enumerate(regs):
                        if rname.startswith('p'):
                            self.out.append('	selp.b32 %%dt2, 1, 0, %%%s;' % rname)
                            rname = '%dt2'
                        self.out.append('	st.b32 [%%dto+%d], %s;' % (4 * k, rname))
                    self.out += ['TRSKIP:', '	add.u32 %dtc, %dtc, 1;', '	setp.ge.u32 %dtq, %dtc, 205;', '	@%dtq exit;']
                if DEBUG:
                    self.out += ['	mov.b32 %d0, %d1;', '	mov.b32 %d1, %d2;', '	mov.b32 %d2, %d3;',
                                 '	mov.b32 %%d3, %d;' % ins.addr,
                                 '	add.u32 %dc, %dc, 1;', '	setp.gt.u32 %dp, %dc, ' + str(DEBUG_LIMIT) + ';',
                                 '	@%dp bra DBGEXIT;']
            self.out.append('\t// %05x  %s' % (ins.addr, ins.text))
            self.one(ins, brk_target, cont_target)
            if ins.join:
                tg = self.jtarget[ins.addr]
                if tg != ins.addr + 8:
                    self.emit_g('bra L_%x;' % tg, ins.guard)
        return self.out

    def one(self, ins, brk_target, cont_target):
        t = ins.toks
        g = ins.guard
        op = t[0]
        self.stats[op] = self.stats.get(op, 0) + 1

        # ---------------- control -------------------
        if op in ('nop', 'joinat'):
            return
        if op in ('prebrk', 'precont'):
            return
        if op == 'bra':
            self.emit_g('bra L_%x;' % int(t[-1], 16), g)
            return
        if op == 'brk':
            self.emit_g('bra L_%x;' % brk_target(ins.addr), g)
            self.targets.add(brk_target(ins.addr))
            return
        if op == 'cont':
            self.emit_g('bra L_%x;' % cont_target(ins.addr), g)
            return
        if op == 'exit':
            self.emit_g('exit;', g)
            return
        if op == 'vote':
            # vote any 0x0 $pD $pS
            assert t[1] == 'any' and t[2] == '0x0', ins.text
            self.emit_g('vote.any.pred %s, %s;' % (self.pred(t[3]), self.pred(t[4])), g)
            return

        # ---------------- moves ---------------------
        if op == 'mov':
            assert t[1] == 'b32'
            d = self.reg(t[2])
            s = t[3]
            if s in ('$clock', '$ctaidx', '$ctaidy', '$tidx', '$tidy'):
                sr = {'$clock': '%clock', '$ctaidx': '%ctaid.x', '$ctaidy': '%ctaid.y',
                      '$tidx': '%tid.x', '$tidy': '%tid.y'}[s]
                if s == '$clock' and CLOCK is not None:
                    sr = str(CLOCK)
                self.emit_g('mov.u32 %s, %s;' % (d, sr), g)
            elif s == 'c1[0x100]':
                # local stack top: lmem base + frame size (the following `sub r1,r1,0x260`
                # lands exactly on the base of our .local array)
                n = self.tmp()
                self.emit('mov.u32 %s, lmem;' % n)
                self.emit_g('add.u32 %s, %s, %d;' % (d, n, self.frame), g)
            else:
                o, _ = self.src(t, 3, 'i')
                self.emit_g('mov.b32 %s, %s;' % (d, o), g)
            return
        if op == 'mov2':
            # logic-unit PASS_B: d = [not] src2 (src1 ignored)
            assert t[1] == 'b32'
            d = self.reg(t[2])
            i = 4  # skip src1
            o, i = self.src(t, i, 'i')
            self.emit_g('mov.b32 %s, %s;' % (d, o), g)
            return

        # ---------------- integer ALU ---------------
        if op in ('add', 'subr', 'sub') and '(' in t:
            return self.int_fused(ins, op)
        if op in ('add', 'sub', 'subr') and t[1] == 'b32':
            d = self.reg(t[2])
            if op == 'add' and len(t) == 5 and t[4] == 'c0[0x4]':
                self.emit_g('cvta.local.u32 %s, %s;' % (d, self.reg(t[3])), g)
                return
            a, i = self.src(t, 3, 'i')
            b, i = self.src(t, i, 'i')
            if op == 'subr':
                a, b = b, a
            self.emit_g('%s.u32 %s, %s, %s;' % ('add' if op == 'add' else 'sub', d, a, b), g)
            return
        if op in ('and', 'or', 'xor'):
            assert t[1] == 'b32'
            d = self.reg(t[2])
            a, i = self.src(t, 3, 'i')
            b, i = self.src(t, i, 'i')
            self.emit_g('%s.b32 %s, %s, %s;' % (op, d, a, b), g)
            return
        if op in ('shl', 'shr'):
            ty = t[1]  # b32 / u32 / s32
            d = self.reg(t[2])
            a, i = self.src(t, 3, 'i')
            mode = t[i]; i += 1
            n, i = self.src(t, i, 'i')
            if mode == 'wrap':
                w = self.tmp()
                self.emit('and.b32 %s, %s, 31;' % (w, n))
                n = w
            else:
                assert mode == 'clamp', ins.text
            ptx = {'b32': 'b32', 'u32': 'u32', 's32': 's32'}[ty]
            self.emit_g('%s.%s %s, %s, %s;' % (op, ptx, d, a, n), g)
            return
        if op == 'mul':
            # mul[ high] [d] sN a sN b   (integer)  -- or float forms handled below
            if t[1] in ('ftz', 'f32'):
                return self.fop(ins)
            hi = False
            i = 1
            if t[i] == 'high':
                hi = True; i += 1
            # printed as:  mul $d u32 a u32 b    or   mul high $d u32 a u32 b
            if t[i].startswith('$r'):
                d = self.reg(t[i]); i += 1
            ty = t[i]; i += 1
            a, i = self.src(t, i, 'i')
            ty2 = t[i]; i += 1
            b, i = self.src(t, i, 'i')
            self.emit_g('mul.%s.%s %s, %s, %s;' % ('hi' if hi else 'lo', ty, d, a, b), g)
            return
        if op == 'slct':
            if t[1] == 'b32':
                d = self.reg(t[2]); a, i = self.src(t, 3, 'i'); b, i = self.src(t, i, 'i')
                cond, ty = t[i], t[i + 1]; i += 2
                c, i = self.src(t, i, 'i')
                q = self.ptmp()
                zero = '0' if ty in ('s32', 'u32') else '0f00000000'
                self.emit('setp.%s.%s %s, %s, %s;' % (cond, ty, q, c, zero))
                self.emit_g('selp.b32 %s, %s, %s, %s;' % (d, a, b, q), g)
                return
        if op == 'selp':
            d = self.reg(t[2]); a, i = self.src(t, 3, 'i'); b, i = self.src(t, i, 'i')
            pn = t[i]
            self.emit_g('selp.b32 %s, %s, %s, %s;' % (d, a, b, self.pred(pn)), g)
            return
        if op == 'cvt':
            return self.cvt(ins)
        if op == 'set':
            return self.setp(ins)

        # ---------------- memory ---------------------
        if op == 'ld':
            return self.ld(ins)
        if op == 'st':
            return self.st(ins)
        if op == 'texfetch':
            return self.tex(ins)

        # ---------------- float ----------------------
        if op in ('add', 'fma', 'min', 'max', 'rcp', 'rsqrt', 'ex2', 'preex2', 'sin', 'cos', 'presin', 'mul'):
            return self.fop(ins)

        raise ValueError('UNHANDLED %05x %s' % (ins.addr, ins.text))

    # ---- fused integer (mad / subr-mul / add-shl)
    def int_fused(self, ins, op):
        t = ins.toks; g = ins.guard
        i = 1
        d_is_prefix = t[1] == 'b32'
        if d_is_prefix:
            i = 2
        d = self.reg(t[i]); i += 1
        # first parenthesised group
        assert t[i] == '('
        j = i + 1
        depth = 1
        k = j
        while depth:
            if t[k] == '(': depth += 1
            elif t[k] == ')': depth -= 1
            k += 1
        grp = t[j:k - 1]
        rest = t[k:]
        c, _ = self.src(rest, 0, 'i')
        if grp[0] == 'shl':
            a, n = self.src(grp, 1, 'i')
            nn, _ = self.src(grp, n, 'i')
            s = self.tmp()
            self.emit('shl.b32 %s, %s, %s;' % (s, a, nn))
            if op == 'add':
                self.emit_g('add.u32 %s, %s, %s;' % (d, s, c), g)
            elif op == 'subr':
                self.emit_g('sub.u32 %s, %s, %s;' % (d, c, s), g)
            else:
                self.emit_g('sub.u32 %s, %s, %s;' % (d, s, c), g)
            return
        assert grp[0] == 'mul', ins.text
        gi = 1
        hi = False
        if grp[gi] == 'high':
            hi = True; gi += 1
        ty = grp[gi]; gi += 1
        a, gi = self.src(grp, gi, 'i')
        gi += 1  # second type token
        b, gi = self.src(grp, gi, 'i')
        if op == 'add':
            self.emit_g('mad.%s.%s %s, %s, %s, %s;' % ('hi' if hi else 'lo', ty, d, a, b, c), g)
        else:
            m = self.tmp()
            self.emit('mul.%s.%s %s, %s, %s;' % ('hi' if hi else 'lo', ty, m, a, b))
            if op == 'subr':
                self.emit_g('sub.u32 %s, %s, %s;' % (d, c, m), g)
            else:
                self.emit_g('sub.u32 %s, %s, %s;' % (d, m, c), g)

    # ---- float ops
    def fop(self, ins):
        t = ins.toks; g = ins.guard
        op = t[0]
        i = 1
        ftz = rn = False
        while t[i] in ('ftz', 'rn', 'rz', 'rm', 'rp', 'sat'):
            if t[i] == 'ftz': ftz = True
            if t[i] == 'rn': rn = True
            assert t[i] in ('ftz', 'rn'), ins.text
            i += 1
        assert t[i] == 'f32', ins.text
        i += 1
        d = self.reg(t[i]); i += 1
        fz = '.ftz' if ftz else ''
        if op in ('add', 'mul'):
            a, i = self.src(t, i, 'f'); b, i = self.src(t, i, 'f')
            self.emit_g('%s.rn%s.f32 %s, %s, %s;' % (op, fz, d, a, b), g)
        elif op == 'fma':
            a, i = self.src(t, i, 'f'); b, i = self.src(t, i, 'f'); c, i = self.src(t, i, 'f')
            self.emit_g('fma.rn%s.f32 %s, %s, %s, %s;' % (fz, d, a, b, c), g)
        elif op in ('min', 'max'):
            a, i = self.src(t, i, 'f'); b, i = self.src(t, i, 'f')
            self.emit_g('%s%s.f32 %s, %s, %s;' % (op, fz, d, a, b), g)
        elif op in ('rcp', 'rsqrt', 'ex2', 'sin', 'cos'):
            a, i = self.src(t, i, 'f')
            self.emit_g('%s.approx.ftz.f32 %s, %s;' % (op, d, a), g)
        elif op in ('preex2', 'presin'):
            # range-reduction helpers: PTX ex2/sin/cos.approx do their own reduction,
            # so keep the raw argument and let the consuming op use it.
            a, i = self.src(t, i, 'f')
            self.emit_g('mov.b32 %s, %s;' % (d, a), g)
        else:
            raise ValueError(ins.text)

    # ---- convert
    def cvt(self, ins):
        t = ins.toks; g = ins.guard
        i = 1
        mods = []
        while t[i] in ('ftz', 'sat', 'rn', 'rz', 'rm', 'rp', 'rzi', 'rmi', 'rpi', 'rni'):
            mods.append(t[i]); i += 1
        dty = t[i]; i += 1
        d = self.reg(t[i]); i += 1
        # source: [neg|abs]* ty [byte] reg
        neg = absf = False
        while t[i] in ('neg', 'abs'):
            if t[i] == 'neg': neg = True
            else: absf = True
            i += 1
        sty = t[i]; i += 1
        if sty == 'u8':
            n = int(t[i], 16); i += 1
            s = self.reg(t[i])
            tt = self.tmp()
            self.emit('shr.u32 %s, %s, %d;' % (tt, s, 8 * n))
            self.emit_g('and.b32 %s, %s, 255;' % (d, tt), g)
            return
        s, _ = self.src(t, i, 'f' if sty == 'f32' else 'i')
        ftz = '.ftz' if 'ftz' in mods else ''
        sat = '.sat' if 'sat' in mods else ''
        rnd = [m for m in mods if m in ('rn', 'rz', 'rm', 'rp', 'rzi', 'rmi', 'rpi', 'rni')]
        rnd = ('.' + rnd[0]) if rnd else ''
        if dty == 'f32' and sty == 'f32':
            if neg and not absf and not sat and not rnd:
                self.emit_g('neg.f32 %s, %s;' % (d, s), g); return
            if absf and not neg and not sat and not rnd:
                self.emit_g('abs.f32 %s, %s;' % (d, s), g); return
            if absf:
                a = self.tmp(); self.emit('abs.f32 %s, %s;' % (a, s)); s = a
            if neg:
                a = self.tmp(); self.emit('neg.f32 %s, %s;' % (a, s)); s = a
            self.emit_g('cvt%s%s%s.f32.f32 %s, %s;' % (rnd, ftz, sat, d, s), g)
            return
        if dty in ('s32', 'u32') and sty in ('s32', 'u32'):
            if absf and not neg:
                self.emit_g('abs.s32 %s, %s;' % (d, s), g); return
            if neg and not absf:
                self.emit_g('neg.s32 %s, %s;' % (d, s), g); return
            if not neg and not absf:
                self.emit_g('mov.b32 %s, %s;' % (d, s), g); return
            raise ValueError(ins.text)
        # float<->int
        if absf or neg:
            raise ValueError(ins.text)
        self.emit_g('cvt%s%s%s.%s.%s %s, %s;' % (rnd, ftz, sat, dty, sty, d, s), g)

    # ---- set predicate
    CMP = {'lt', 'le', 'gt', 'ge', 'eq', 'ne', 'neu', 'equ', 'ltu', 'leu', 'gtu', 'geu', 'num', 'nan'}

    def setp(self, ins):
        t = ins.toks; g = ins.guard
        i = 1
        ftz = False
        if t[i] == 'ftz':
            ftz = True; i += 1
        pd = self.pred(t[i]); i += 1
        assert t[i] == '0x1', 'second dst predicate unsupported: ' + ins.text
        i += 1

        def pterm(i):
            neg = False
            if t[i] == 'not':
                neg = True; i += 1
            tk = t[i]; i += 1
            if tk == '0x1':
                return ('true', neg), i
            return (self.pred(tk), neg), i

        def lit(term):
            nm, neg = term
            if nm == 'true':
                p = self.ptmp()
                self.emit('setp.eq.u32 %s, 0, %s;' % (p, '0' if neg else '1') ) if False else None
                # constant: encode as compare 1==1 / 1!=1
                self.emit('setp.%s.u32 %s, 1, 1;' % ('ne' if neg else 'eq', p))
                return p
            if neg:
                p = self.ptmp()
                self.emit('not.pred %s, %s;' % (p, nm))
                return p
            return nm

        if t[i] in self.CMP:
            cond = t[i]; ty = t[i + 1]; i += 2
            kind = 'f' if ty == 'f32' else 'i'
            a, i = self.src(t, i, kind)
            b, i = self.src(t, i, kind)
            cp = self.ptmp()
            fz = '.ftz' if (ftz and ty == 'f32') else ''
            self.emit('setp.%s%s.%s %s, %s, %s;' % (cond, fz, ty, cp, a, b))
            cur = cp
        else:
            term, i = pterm(i)
            cur = lit(term)
            # first boolean op follows
            while i < len(t) and t[i] in ('and', 'or', 'xor'):
                o = t[i]; i += 1
                term2, i = pterm(i)
                r = self.ptmp()
                self.emit('%s.pred %s, %s, %s;' % (o, r, cur, lit(term2)))
                cur = r
            self.emit_g('mov.pred %s, %s;' % (pd, cur), g)
            return
        while i < len(t) and t[i] in ('and', 'or', 'xor'):
            o = t[i]; i += 1
            term2, i = pterm(i)
            r = self.ptmp()
            self.emit('%s.pred %s, %s, %s;' % (o, r, cur, lit(term2)))
            cur = r
        assert i == len(t), ins.text
        self.emit_g('mov.pred %s, %s;' % (pd, cur), g)

    # ---- memory
    def ld(self, ins):
        t = ins.toks; g = ins.guard
        w = t[1]
        n, cnt = self.regn(t[2])
        m = t[3]
        if m in ('ca', 'cs', 'cg', 'cv'):
            m = t[4]
        # constant bank load: ld b32 R c2[R]
        if re.fullmatch(r'c\d+\[.*\]', m):
            o = self.base(m, 'i')
            self.emit_g('mov.b32 %%r%d, %s;' % (n, o), g)
            return
        sp, a = self.addr(m)
        space = 'local' if sp == 'l' else ''
        sp_s = ('.' + space) if space else ''
        if w == 'b32':
            self.emit_g('ld%s.b32 %%r%d, %s;' % (sp_s, n, a), g)
        elif w == 'b64':
            self.emit_g('ld%s.v2.b32 {%%r%d, %%r%d}, %s;' % (sp_s, n, n + 1, a), g)
        elif w == 'b128':
            self.emit_g('ld%s.v4.b32 {%%r%d, %%r%d, %%r%d, %%r%d}, %s;' % (sp_s, n, n + 1, n + 2, n + 3, a), g)
        else:
            raise ValueError(ins.text)

    def st(self, ins):
        t = ins.toks; g = ins.guard
        w = t[1]
        i = 2
        if t[i] in ('wb', 'cg', 'cs', 'wt'):
            i += 1
        sp, a = self.addr(t[i]); i += 1
        n, cnt = self.regn(t[i])
        sp_s = '.local' if sp == 'l' else ''
        if w == 'b32':
            self.emit_g('st%s.b32 %s, %%r%d;' % (sp_s, a, n), g)
        elif w == 'b64':
            self.emit_g('st%s.v2.b32 %s, {%%r%d, %%r%d};' % (sp_s, a, n, n + 1), g)
        elif w == 'b128':
            self.emit_g('st%s.v4.b32 %s, {%%r%d, %%r%d, %%r%d, %%r%d};' % (sp_s, a, n, n + 1, n + 2, n + 3), g)
        else:
            raise ValueError(ins.text)

    def tex(self, ins):
        # texfetch t lzero all $a:$b:$c:$d t1d $t0 $s0 $coord ()
        t = ins.toks; g = ins.guard
        assert t[1] == 't' and t[2] == 'lzero' and t[3] == 'all' and t[5] == 't1d' and t[6] == '$t0', ins.text
        parts = t[4].split(':')
        dst = []
        for p in parts:
            dst.append('%%r%s' % p[2:] if p.startswith('$r') else self.tmp())
        coord = self.reg(t[8])
        self.emit_g('tex.1d.v4.f32.s32 {%s}, [t_bvh, {%s}];' % (', '.join(dst), coord), g)


# --------------------------------------------------------------------------
def translate(cubin_path, out_path):
    secs = read_cubin(cubin_path)
    kernels = [n[len('.text.'):] for n in secs if n.startswith('.text.')]
    ctx = secs['.nv.constant2']
    hdr = ['//', '// Generated by sass2ptx.py from TracerCUDA_sm20.cubin (Fermi sm_20 SASS).',
           '//', '.version 4.3', '.target sm_50', '.address_size 32', '',
           '.const .align 16 .b8 context[%d];' % len(ctx),
           '.global .texref t_bvh;', '']
    body = []
    allstats = {}
    for k in kernels:
        insns = parse_lines(disassemble(secs['.text.' + k]))
        x = Xlat(k, insns, secs['.nv.constant16.' + k])
        x.frame = 0x260
        lines = x.run()
        for kk, vv in x.stats.items():
            allstats[kk] = allstats.get(kk, 0) + vv
        body.append('.visible .entry %s()' % k)
        body.append('{')
        body.append('\t.reg .b32 %r<64>;')
        body.append('\t.reg .b32 %%t<%d>;' % max(x.maxtmp, 1))
        body.append('\t.reg .pred %p<8>;')
        body.append('\t.reg .pred %%q<%d>;' % max(x.maxtp, 1))
        body.append('\t.local .align 16 .b8 lmem[%d];' % 0x300)
        if TRACE:
            body.append('	.reg .b32 %dt, %dt2, %dtc, %dto; .reg .pred %dtp, %dtq;')
            body.append('	mov.b32 %dtc, 0;')
        if DEBUG:
            body.append('	.reg .b32 %d<4>; .reg .b32 %dc; .reg .pred %dp; .reg .b32 %dt;')
            body.append('	mov.b32 %dc, 0; mov.b32 %d0, 0; mov.b32 %d1, 0; mov.b32 %d2, 0; mov.b32 %d3, 0;')
        body += lines
        if DEBUG:
            body += ['DBGEXIT:', '	mov.u32 %dt, %ctaid.x;', '	mul.lo.u32 %dt, %dt, 64;', '	mov.u32 %r0, %tid.y;',
                     '	mad.lo.u32 %dt, %r0, 32, %dt;', '	mov.u32 %r0, %tid.x;', '	add.u32 %dt, %dt, %r0;',
                     '	shl.b32 %dt, %dt, 4;', '	ld.const.b32 %r0, [context+544];', '	add.u32 %dt, %dt, %r0;',
                     '	st.b32 [%dt], %d0;', '	st.b32 [%dt+4], %d1;', '	st.b32 [%dt+8], %d2;', '	st.b32 [%dt+12], %d3;', '	exit;']
        body.append('}')
        body.append('')
    open(out_path, 'w').write('\n'.join(hdr + body))
    return allstats


if __name__ == '__main__':
    cubin = sys.argv[1]
    out = sys.argv[2]
    if '--clock' in sys.argv:
        CLOCK = int(sys.argv[sys.argv.index('--clock') + 1], 0)
    if len(sys.argv) > 3 and sys.argv[3] == '--debug':
        DEBUG = True
    if len(sys.argv) > 3 and sys.argv[3] == '--trace':
        a, rr = sys.argv[4].split(':')
        TRACE = (int(a, 16), ['%%r%s' % x if x.isdigit() else x for x in rr.split(',')])
        DEBUG = False
    st = translate(cubin, out)
    print('wrote', out, sorted(st.items(), key=lambda x: -x[1])[:12])
