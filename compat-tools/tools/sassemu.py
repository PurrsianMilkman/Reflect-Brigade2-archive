#!/usr/bin/env python3
"""
sassemu.py - tiny single-thread interpreter for the Fermi SASS of TracePixelMIS /
TracePixelReflect.  Independent of sass2ptx.py's PTX emission: it evaluates the
disassembly directly so the two can be diffed instruction-by-instruction.

usage: sassemu.py <kernel> <tid_x> <tid_y> <ctaid_x> [--trace 0xADDR:r52,r53,...] [--max N] [--clock C]
"""
import sys, struct, re, os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sass2ptx as S

F32 = np.float32
M = 0xffffffff
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
WORK = os.path.join(ROOT, 'work')


def f2b(x): return struct.unpack('<I', struct.pack('<f', float(x)))[0]
def b2f(b): return struct.unpack('<f', struct.pack('<I', b & M))[0]
def s32(x):
    x &= M
    return x - (1 << 32) if x & 0x80000000 else x
def ftz(x):
    x = F32(x)
    if x != 0 and abs(x) < F32(1.17549435e-38): return F32(0.0) if np.signbit(x) == 0 else F32(-0.0)
    return x


class Emu:
    def __init__(self, kernel, tx, ty, cx, clock=0x12345):
        secs = S.read_cubin(os.path.join(ROOT, '..', 'reflect_v2', 'bin', 'TracerCUDA_sm20.cubin'))
        self.insns = S.parse_lines(S.disassemble(secs['.text.' + kernel]))
        self.c16 = secs['.nv.constant16.' + kernel]
        self.ctx = bytearray(open(os.path.join(WORK, 'context.bin'), 'rb').read())
        self.bvh = open(os.path.join(WORK, 'bvh_full.bin'), 'rb').read()
        self.mat = open(os.path.join(WORK, 'dump_mat_off0_sz6800.bin'), 'rb').read()
        self.lit = open(os.path.join(WORK, 'dump_light_off0_sz32768.bin'), 'rb').read()
        self.sky = open(os.path.join(WORK, 'sky.rgba'), 'rb').read()
        self.r = [0] * 64
        self.p = [False] * 8
        self.lmem = bytearray(0x400)
        self.LBASE = 0x3fff000       # local window base == generic address of lmem[0]
        self.tx, self.ty, self.cx, self.clock = tx, ty, cx, clock
        self.gmem = {}               # generic stores to non-local, sparse
        self.byaddr = {i.addr: k for k, i in enumerate(self.insns)}
        self.steps = 0

    # ---- memory
    def rd32(self, a):
        a &= M
        if self.LBASE <= a < self.LBASE + len(self.lmem):
            return struct.unpack_from('<I', self.lmem, a - self.LBASE)[0]
        for base, blob in ((struct.unpack_from('<I', self.ctx, 0x224)[0], self.bvh),
                           (struct.unpack_from('<I', self.ctx, 0x228)[0], self.mat),
                           (struct.unpack_from('<I', self.ctx, 0x22C)[0], self.lit)):
            if base <= a < base + len(blob):
                return struct.unpack_from('<I', blob, a - base)[0]
        if a in self.gmem: return self.gmem[a]
        return 0x80808080

    def wr32(self, a, v):
        a &= M
        if self.LBASE <= a < self.LBASE + len(self.lmem):
            struct.pack_into('<I', self.lmem, a - self.LBASE, v & M)
        else:
            self.gmem[a] = v & M

    # ---- operand eval
    def val(self, toks, i, kind):
        neg = absf = nt = False
        while toks[i] in ('neg', 'abs', 'not'):
            if toks[i] == 'neg': neg = True
            elif toks[i] == 'abs': absf = True
            else: nt = True
            i += 1
        t = toks[i]; i += 1
        v = self.base(t)
        if absf: v = (f2b(abs(b2f(v))) if kind == 'f' else abs(s32(v)) & M)
        if neg: v = (v ^ 0x80000000) if kind == 'f' else (-s32(v)) & M
        if nt: v = ~v & M
        return v, i

    def base(self, t):
        if t.startswith('$r'): return self.r[int(re.match(r'\$r(\d+)', t).group(1))] & M
        if re.fullmatch(r'-?0x[0-9a-f]+', t): return int(t, 16) & M
        m = re.fullmatch(r'c(\d+)\[(.*)\]', t)
        bank, idx = int(m.group(1)), m.group(2)
        if bank == 16: return struct.unpack_from('<I', self.c16, int(idx, 16))[0]
        if bank == 2:
            off = int(idx, 16) if idx.startswith('0x') else self.r[int(idx[2:])]
            return struct.unpack_from('<I', self.ctx, off)[0]
        if bank == 0:
            return {0xc: 2, 0x14: 2400, 0x8: 32, 0x4: self.LBASE}[int(idx, 16)]
        raise ValueError(t)

    def setr(self, t, v):
        self.r[int(re.match(r'\$r(\d+)', t).group(1))] = v & M

    def guard_ok(self, g):
        if g is None: return True
        v = self.p[int(g[0][1])]
        return (not v) if g[1] else v

    def memaddr(self, t):
        m = re.fullmatch(r'[lg]\[\$r(\d+)(?:([+-])(0x[0-9a-f]+))?\]', t)
        a = self.r[int(m.group(1))]
        if m.group(3): a += (1 if m.group(2) == '+' else -1) * int(m.group(3), 16)
        sp = t[0]
        if sp == 'l': a += self.LBASE
        return a & M

    # ---- run
    def run(self, trace=None, maxsteps=2000000, maxtrace=40):
        pc = 0
        log = []
        n = len(self.insns)
        while pc < n and self.steps < maxsteps:
            ins = self.insns[pc]
            self.steps += 1
            if trace and ins.addr == trace[0] and len(log) < maxtrace:
                log.append(self.snapshot(trace[1]))
            nxt = self.step(ins, pc)
            if nxt == 'exit': break
            pc = nxt if nxt is not None else pc + 1
        return log

    def snapshot(self, names):
        out = []
        for nm in names:
            if nm.startswith('p'): out.append(int(self.p[int(nm[1:])]))
            else: out.append(self.r[int(nm[1:])])
        return out

    # prebrk / precont stacks resolved dynamically here (true stack semantics)
    brkstack = None

    def step(self, ins, pc):
        t = ins.toks; op = t[0]
        if self.brkstack is None:
            self.brkstack = []   # unified SIMT stack of (kind, target)
            self.joinlog = {}
        ok = self.guard_ok(ins.guard)
        nxt = self.step_core(ins, pc, t, op, ok)
        if ins.join and ok:
            kind, tg = self.brkstack.pop()
            assert kind == 's', 'join popped %s at %x' % (kind, ins.addr)
            self.joinlog[ins.addr] = tg
            if nxt is None: return self.byaddr[tg]
        return nxt

    def step_core(self, ins, pc, t, op, ok):
        if op == 'joinat': self.brkstack.append(('s', int(t[1], 16))); return None
        if op == 'nop': return None
        if op == 'prebrk': self.brkstack.append(('b', int(t[1], 16))); return None
        if op == 'precont': self.brkstack.append(('c', int(t[1], 16))); return None
        if not ok: return None
        if op == 'bra': return self.byaddr[int(t[-1], 16)]
        if op == 'brk':
            while self.brkstack[-1][0] != 'b': self.brkstack.pop()
            tg = self.brkstack.pop()[1]; return self.byaddr[tg]
        if op == 'cont':
            while self.brkstack[-1][0] != 'c': self.brkstack.pop()
            return self.byaddr[self.brkstack.pop()[1]]
        if op == 'exit': return 'exit'
        if op == 'vote':
            self.p[int(t[3][2])] = self.p[int(t[4][2])]; return None
        if op == 'mov':
            s = t[3]
            if s == '$clock': v = self.clock
            elif s == '$ctaidx': v = self.cx
            elif s == '$ctaidy': v = 0
            elif s == '$tidx': v = self.tx
            elif s == '$tidy': v = self.ty
            elif s == 'c1[0x100]': v = 0x260   # lmem offset space: r1 = top; sub 0x260 -> 0
            else: v, _ = self.val(t, 3, 'i')
            self.setr(t[2], v); return None
        if op == 'mov2':
            v, _ = self.val(t, 4, 'i'); self.setr(t[2], v); return None
        if op in ('add', 'sub', 'subr') and '(' in t:
            return self.fused(ins, op)
        if op in ('add', 'sub', 'subr') and t[1] == 'b32':
            if op == 'add' and t[4] == 'c0[0x4]':
                self.setr(t[2], self.r[int(t[3][2:])] + self.LBASE); return None
            a, i = self.val(t, 3, 'i'); b, i = self.val(t, i, 'i')
            if op == 'subr': a, b = b, a
            self.setr(t[2], a + b if op == 'add' else a - b); return None
        if op in ('and', 'or', 'xor') and t[1] == 'b32':
            a, i = self.val(t, 3, 'i'); b, i = self.val(t, i, 'i')
            self.setr(t[2], {'and': a & b, 'or': a | b, 'xor': a ^ b}[op]); return None
        if op in ('shl', 'shr'):
            a, i = self.val(t, 3, 'i'); mode = t[i]; i += 1; nn, i = self.val(t, i, 'i')
            if mode == 'wrap': nn &= 31
            if op == 'shl': v = (a << nn) & M if nn < 32 else 0
            elif t[1] == 's32': v = (s32(a) >> min(nn, 31)) & M
            else: v = a >> nn if nn < 32 else 0
            self.setr(t[2], v); return None
        if op == 'mul' and t[1] not in ('ftz', 'f32'):
            i = 1; hi = False
            if t[i] == 'high': hi = True; i += 1
            d = t[i]; i += 1; ty = t[i]; i += 1
            a, i = self.val(t, i, 'i'); i += 1; b, i = self.val(t, i, 'i')
            if ty == 's32': p = s32(a) * s32(b)
            else: p = a * b
            self.setr(d, (p >> 32) if hi else p); return None
        if op == 'slct' and t[1] == 'b32':
            a, i = self.val(t, 3, 'i'); b, i = self.val(t, i, 'i'); cond, ty = t[i], t[i + 1]; i += 2
            c, i = self.val(t, i, 'i')
            cv = s32(c) if ty == 's32' else c
            r = {'eq': cv == 0, 'lt': cv < 0, 'ne': cv != 0, 'le': cv <= 0, 'gt': cv > 0, 'ge': cv >= 0}[cond]
            self.setr(t[2], a if r else b); return None
        if op == 'selp':
            a, i = self.val(t, 3, 'i'); b, i = self.val(t, i, 'i')
            self.setr(t[2], a if self.p[int(t[i][2])] else b); return None
        if op == 'cvt': return self.cvt(ins)
        if op == 'set': return self.setp(ins)
        if op == 'ld': return self.ld(ins)
        if op == 'st': return self.st(ins)
        if op == 'texfetch': return self.tex(ins)
        return self.fop(ins)

    def fused(self, ins, op):
        t = ins.toks
        i = 2 if t[1] == 'b32' else 1
        d = t[i]; i += 1
        j = i + 1; depth = 1; k = j
        while depth:
            if t[k] == '(': depth += 1
            elif t[k] == ')': depth -= 1
            k += 1
        grp = t[j:k - 1]; rest = t[k:]
        c, _ = self.val(rest, 0, 'i')
        if grp[0] == 'shl':
            a, n = self.val(grp, 1, 'i'); nn, _ = self.val(grp, n, 'i')
            prod = (a << nn) & M if nn < 32 else 0
        else:
            gi = 1; hi = False
            if grp[gi] == 'high': hi = True; gi += 1
            ty = grp[gi]; gi += 1
            a, gi = self.val(grp, gi, 'i'); gi += 1; b, gi = self.val(grp, gi, 'i')
            p = (s32(a) * s32(b)) if ty == 's32' else a * b
            prod = (p >> 32) if hi else p
        if op == 'add': v = prod + c
        elif op == 'subr': v = c - prod
        else: v = prod - c
        self.setr(d, v); return None

    def fop(self, ins):
        t = ins.toks; op = t[0]
        i = 1
        fz = False
        while t[i] in ('ftz', 'rn'):
            if t[i] == 'ftz': fz = True
            i += 1
        i += 1  # f32
        d = t[i]; i += 1
        def g(x):
            x = b2f(x)
            return ftz(x) if fz else F32(x)
        def put(x):
            x = F32(x)
            if fz: x = ftz(x)
            self.setr(d, f2b(x))
        with np.errstate(all='ignore'):
            if op == 'add':
                a, i = self.val(t, i, 'f'); b, i = self.val(t, i, 'f'); put(g(a) + g(b))
            elif op == 'mul':
                a, i = self.val(t, i, 'f'); b, i = self.val(t, i, 'f'); put(g(a) * g(b))
            elif op == 'fma':
                a, i = self.val(t, i, 'f'); b, i = self.val(t, i, 'f'); c, i = self.val(t, i, 'f')
                put(F32(float(g(a)) * float(g(b)) + float(g(c))))
            elif op in ('min', 'max'):
                a, i = self.val(t, i, 'f'); b, i = self.val(t, i, 'f'); x, y = g(a), g(b)
                if np.isnan(x): r = y
                elif np.isnan(y): r = x
                else: r = min(x, y) if op == 'min' else max(x, y)
                put(r)
            elif op == 'rcp':
                a, i = self.val(t, i, 'f'); x = g(a); put(F32(1.0) / x if x != 0 else F32(np.copysign(np.inf, x)))
            elif op == 'rsqrt':
                a, i = self.val(t, i, 'f'); put(F32(1.0) / np.sqrt(g(a)))
            elif op in ('preex2', 'presin'):
                a, i = self.val(t, i, 'f'); self.setr(d, a)
            elif op == 'ex2':
                a, i = self.val(t, i, 'f'); put(np.exp2(g(a)))
            elif op == 'sin':
                a, i = self.val(t, i, 'f'); put(np.sin(g(a)))
            elif op == 'cos':
                a, i = self.val(t, i, 'f'); put(np.cos(g(a)))
            else:
                raise ValueError(ins.text)
        return None

    def cvt(self, ins):
        t = ins.toks; i = 1; mods = []
        while t[i] in ('ftz', 'sat', 'rn', 'rz', 'rm', 'rp', 'rzi', 'rmi'):
            mods.append(t[i]); i += 1
        dty = t[i]; i += 1; d = t[i]; i += 1
        neg = absf = False
        while t[i] in ('neg', 'abs'):
            if t[i] == 'neg': neg = True
            else: absf = True
            i += 1
        sty = t[i]; i += 1
        if sty == 'u8':
            n = int(t[i], 16); i += 1
            self.setr(d, (self.base(t[i]) >> (8 * n)) & 255); return None
        v, _ = self.val(t, i, 'f' if sty == 'f32' else 'i')
        import math
        if sty == 'f32':
            x = b2f(v)
            if 'ftz' in mods: x = float(ftz(x))
            if absf: x = abs(x)
            if neg: x = -x
        if dty == 'f32' and sty == 'f32':
            if 'rmi' in mods and not math.isnan(x) and not math.isinf(x): x = math.floor(x)
            if 'sat' in mods: x = 0.0 if math.isnan(x) else min(1.0, max(0.0, x))
            self.setr(d, f2b(x)); return None
        if dty == 'f32':
            iv = s32(v) if sty == 's32' else v
            if absf: iv = abs(iv)
            if neg: iv = -iv
            f = F32(iv)
            if 'rp' in mods:
                if float(f) < iv: f = np.nextafter(f, F32(np.inf))
            self.setr(d, f2b(f)); return None
        if sty == 'f32':   # float -> int, truncating, saturating
            if math.isnan(x): r = 0
            else:
                r = int(math.trunc(x)) if not math.isinf(x) else (1 << 40) * (1 if x > 0 else -1)
                lo, hi = (-(1 << 31), (1 << 31) - 1) if dty == 's32' else (0, M)
                r = max(lo, min(hi, r))
            self.setr(d, r); return None
        iv = s32(v)
        if absf: iv = abs(iv)
        if neg: iv = -iv
        self.setr(d, iv); return None

    CMPF = {'lt': lambda a, b: a < b, 'le': lambda a, b: a <= b, 'gt': lambda a, b: a > b, 'ge': lambda a, b: a >= b,
            'eq': lambda a, b: a == b, 'ne': lambda a, b: a != b}

    def setp(self, ins):
        t = ins.toks; i = 1
        if t[i] == 'ftz': i += 1
        pd = int(t[i][2]); i += 2

        def term(i):
            neg = False
            if t[i] == 'not': neg = True; i += 1
            tk = t[i]; i += 1
            v = True if tk == '0x1' else self.p[int(tk[2])]
            return (not v if neg else v), i
        if t[i] in self.CMPF or t[i] == 'neu':
            cond, ty = t[i], t[i + 1]; i += 2
            kind = 'f' if ty == 'f32' else 'i'
            a, i = self.val(t, i, kind); b, i = self.val(t, i, kind)
            if ty == 'f32':
                x, y = b2f(a), b2f(b)
                x, y = float(ftz(x)), float(ftz(y))
                if cond == 'neu': cur = (x != y) or np.isnan(x) or np.isnan(y)
                else: cur = False if (np.isnan(x) or np.isnan(y)) else self.CMPF[cond](x, y)
            else:
                x, y = (s32(a), s32(b)) if ty == 's32' else (a, b)
                cur = self.CMPF[cond](x, y)
        else:
            cur, i = term(i)
            while i < len(t) and t[i] in ('and', 'or', 'xor'):
                o = t[i]; i += 1; v2, i = term(i)
                cur = {'and': cur and v2, 'or': cur or v2, 'xor': cur != v2}[o]
            self.p[pd] = cur; return None
        while i < len(t) and t[i] in ('and', 'or', 'xor'):
            o = t[i]; i += 1; v2, i = term(i)
            cur = {'and': cur and v2, 'or': cur or v2, 'xor': cur != v2}[o]
        self.p[pd] = bool(cur); return None

    def ld(self, ins):
        t = ins.toks; w = t[1]; n = int(re.match(r'\$r(\d+)', t[2]).group(1)); m = t[3]
        if m in ('ca', 'cs'): m = t[4]
        if m.startswith('c'):
            self.r[n] = self.base(m); return None
        a = self.memaddr(m)
        cnt = {'b32': 1, 'b64': 2, 'b128': 4}[w]
        for k in range(cnt):
            self.r[n + k] = self.rd32(a + 4 * k)
        return None

    def st(self, ins):
        t = ins.toks; w = t[1]; i = 2
        if t[i] in ('wb',): i += 1
        a = self.memaddr(t[i]); i += 1
        n = int(re.match(r'\$r(\d+)', t[i]).group(1))
        cnt = {'b32': 1, 'b64': 2, 'b128': 4}[w]
        for k in range(cnt):
            self.wr32(a + 4 * k, self.r[n + k])
        return None

    def tex(self, ins):
        t = ins.toks
        parts = t[4].split(':'); idx = self.r[int(t[8][2:])]
        for k, p in enumerate(parts):
            if p.startswith('$r'):
                self.r[int(p[2:])] = struct.unpack_from('<I', self.bvh, idx * 16 + 4 * k)[0]
        return None


if __name__ == '__main__':
    a = sys.argv
    kernel = a[1]; tx, ty, cx = int(a[2]), int(a[3]), int(a[4])
    trace = None
    if '--trace' in a:
        ad, rr = a[a.index('--trace') + 1].split(':')
        trace = (int(ad, 16), rr.split(','))
    mx = int(a[a.index('--max') + 1]) if '--max' in a else 3000000
    clock = int(a[a.index('--clock') + 1], 0) if '--clock' in a else 0x12345
    e = Emu(kernel, tx, ty, cx, clock)
    log = e.run(trace, mx)
    print('steps', e.steps, 'finished' if e.steps < mx else 'LIMIT')
    for k, row in enumerate(log):
        print(k, row)
