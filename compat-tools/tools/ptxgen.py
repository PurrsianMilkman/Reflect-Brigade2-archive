"""
Minimal PTX emitter for hand-authoring 32-bit CUDA kernels without a compiler.

PTX registers are virtual, so we don't need real register allocation - we just
name them and let ptxas (inside the driver JIT) do the work. This layer gives us
expression-style code, typed temporaries, float3 helpers and structured labels,
which is what makes a path tracer feasible to write by hand.
"""

class Reg:
    __slots__ = ("name", "ty")
    def __init__(self, name, ty):
        self.name, self.ty = name, ty
    def __repr__(self):
        return self.name


class Kernel:
    def __init__(self, name):
        self.name = name
        self.body = []
        self.n = {"f32": 0, "u32": 0, "s32": 0, "pred": 0}
        self._lbl = 0

    # ---- registers ---------------------------------------------------------
    def _new(self, ty):
        pfx = {"f32": "%f", "u32": "%r", "s32": "%r", "pred": "%p"}[ty]
        # u32 and s32 share the %r bank
        key = "u32" if ty in ("u32", "s32") else ty
        i = self.n[key]; self.n[key] = i + 1
        return Reg("%s%d" % (pfx, i), ty)

    def f(self, init=None):
        r = self._new("f32")
        if init is not None:
            self.raw("mov.f32 %s, %s;" % (r, self.fimm(init)))
        return r

    def u(self, init=None):
        r = self._new("u32")
        if init is not None:
            self.raw("mov.u32 %s, %d;" % (r, init))
        return r

    def pred(self):
        return self._new("pred")

    @staticmethod
    def fimm(x):
        """PTX float immediates are hex bit patterns: 0f3F800000"""
        import struct
        if isinstance(x, str):
            return x
        return "0f%08X" % struct.unpack("<I", struct.pack("<f", float(x)))[0]

    # ---- emission ----------------------------------------------------------
    def raw(self, line):
        self.body.append("\t" + line)

    def comment(self, c):
        self.body.append("\t// " + c)

    def label(self, hint="L"):
        self._lbl += 1
        return "%s%d" % (hint, self._lbl)

    def place(self, lbl):
        self.body.append(lbl + ":")

    def bra(self, lbl, pred=None, neg=False):
        if pred is None:
            self.raw("bra %s;" % lbl)
        else:
            self.raw("@%s%s bra %s;" % ("!" if neg else "", pred, lbl))

    # ---- float ops ---------------------------------------------------------
    def _bin_f(self, op, a, b):
        d = self._new("f32")
        self.raw("%s.f32 %s, %s, %s;" % (op, d, a, self._op(b)))
        return d

    def _op(self, x):
        return self.fimm(x) if isinstance(x, (int, float)) else x

    def fadd(self, a, b): return self._bin_f("add", a, b)
    def fsub(self, a, b): return self._bin_f("sub", a, b)
    def fmul(self, a, b): return self._bin_f("mul", a, b)
    def fmin(self, a, b): return self._bin_f("min", a, b)
    def fmax(self, a, b): return self._bin_f("max", a, b)

    def fdiv(self, a, b):
        d = self._new("f32")
        self.raw("div.rn.f32 %s, %s, %s;" % (d, self._op(a), self._op(b)))
        return d

    def frcp(self, a):
        d = self._new("f32")
        self.raw("rcp.rn.f32 %s, %s;" % (d, a))
        return d

    def fma(self, a, b, c):
        d = self._new("f32")
        self.raw("fma.rn.f32 %s, %s, %s, %s;" % (d, self._op(a), self._op(b), self._op(c)))
        return d

    def fneg(self, a):
        d = self._new("f32")
        self.raw("neg.f32 %s, %s;" % (d, a))
        return d

    def frsqrt(self, a):
        d = self._new("f32")
        self.raw("rsqrt.approx.f32 %s, %s;" % (d, a))
        return d

    def fmov(self, a):
        d = self._new("f32")
        self.raw("mov.f32 %s, %s;" % (d, self._op(a)))
        return d

    def fset(self, dst, a):
        self.raw("mov.f32 %s, %s;" % (dst, self._op(a)))

    # ---- int ops -----------------------------------------------------------
    def _bin_i(self, op, a, b, ty="u32"):
        d = self._new("u32")
        bb = b if isinstance(b, Reg) else str(b)
        self.raw("%s.%s %s, %s, %s;" % (op, ty, d, a, bb))
        return d

    def iadd(self, a, b): return self._bin_i("add", a, b, "s32")
    def isub(self, a, b): return self._bin_i("sub", a, b, "s32")
    def imul(self, a, b): return self._bin_i("mul.lo", a, b, "s32")
    def ishl(self, a, b): return self._bin_i("shl", a, b, "b32")
    def ishr(self, a, b): return self._bin_i("shr", a, b, "s32")
    def iand(self, a, b): return self._bin_i("and", a, b, "b32")
    def idiv(self, a, b): return self._bin_i("div", a, b, "u32")

    def imov(self, a):
        d = self._new("u32")
        self.raw("mov.u32 %s, %s;" % (d, a if isinstance(a, Reg) else str(a)))
        return d

    def iset(self, dst, a):
        self.raw("mov.u32 %s, %s;" % (dst, a if isinstance(a, Reg) else str(a)))

    def cvt_f(self, u):
        d = self._new("f32")
        self.raw("cvt.rn.f32.s32 %s, %s;" % (d, u))
        return d

    def bitcast_f(self, u):
        d = self._new("f32")
        self.raw("mov.b32 %s, %s;" % (d, u))
        return d

    def bitcast_i(self, f):
        d = self._new("u32")
        self.raw("mov.b32 %s, %s;" % (d, f))
        return d

    # ---- predicates --------------------------------------------------------
    def setp(self, cmp_, a, b, ty="f32"):
        p = self.pred()
        bb = self._op(b) if ty == "f32" else (b if isinstance(b, Reg) else str(b))
        self.raw("setp.%s.%s %s, %s, %s;" % (cmp_, ty, p, a, bb))
        return p

    def selp_f(self, p, a, b):
        d = self._new("f32")
        self.raw("selp.f32 %s, %s, %s, %s;" % (d, self._op(a), self._op(b), p))
        return d

    def selp_i(self, p, a, b):
        d = self._new("u32")
        self.raw("selp.b32 %s, %s, %s, %s;" % (d, a, b, p))
        return d

    # ---- memory ------------------------------------------------------------
    def ldc_u(self, off):
        d = self._new("u32")
        self.raw("ld.const.u32 %s, [context+%d];" % (d, off))
        return d

    def ldc_f(self, off):
        d = self._new("f32")
        self.raw("ld.const.f32 %s, [context+%d];" % (d, off))
        return d

    def ldc_v3(self, off):
        return (self.ldc_f(off), self.ldc_f(off + 4), self.ldc_f(off + 8))

    def ldg_f(self, addr, off=0):
        d = self._new("f32")
        self.raw("ld.global.f32 %s, [%s+%d];" % (d, addr, off))
        return d

    def ldg_u(self, addr, off=0):
        d = self._new("u32")
        self.raw("ld.global.u32 %s, [%s+%d];" % (d, addr, off))
        return d

    def ldg_v4(self, addr, off=0):
        a, b, c, e = (self._new("f32") for _ in range(4))
        self.raw("ld.global.v4.f32 {%s,%s,%s,%s}, [%s+%d];" % (a, b, c, e, addr, off))
        return (a, b, c, e)

    def stg_v4(self, addr, v, off=0):
        self.raw("st.global.v4.f32 [%s+%d], {%s,%s,%s,%s};" %
                 (addr, off, self._op(v[0]), self._op(v[1]), self._op(v[2]), self._op(v[3])))

    # ---- float3 helpers ----------------------------------------------------
    def v3(self, x, y, z):
        return (x, y, z)

    def v3add(self, a, b): return tuple(self.fadd(a[i], b[i]) for i in range(3))
    def v3sub(self, a, b): return tuple(self.fsub(a[i], b[i]) for i in range(3))

    def v3scale(self, a, s):
        return tuple(self.fmul(a[i], s) for i in range(3))

    def v3mad(self, a, b, s):
        """a + b*s"""
        return tuple(self.fma(b[i], s, a[i]) for i in range(3))

    def v3dot(self, a, b):
        t = self.fmul(a[0], b[0])
        t = self.fma(a[1], b[1], t)
        return self.fma(a[2], b[2], t)

    def v3norm(self, a):
        d = self.v3dot(a, a)
        r = self.frsqrt(d)
        return self.v3scale(a, r)

    # ---- assembly ----------------------------------------------------------
    def render(self):
        decls = []
        if self.n["f32"]:  decls.append("\t.reg .f32 %%f<%d>;" % (self.n["f32"] + 1))
        if self.n["u32"]:  decls.append("\t.reg .u32 %%r<%d>;" % (self.n["u32"] + 1))
        if self.n["pred"]: decls.append("\t.reg .pred %%p<%d>;" % (self.n["pred"] + 1))
        return (".visible .entry %s()\n{\n" % self.name
                + "\n".join(decls) + "\n\n"
                + "\n".join(self.body)
                + "\n\tret;\n}\n")


def module(kernels, const_bytes=656):
    out = []
    out.append("//")
    out.append("// Generated by ptxgen.py - 32-bit PTX for Brigade 2 TracerCUDA")
    out.append("//")
    out.append(".version 4.0")
    out.append(".target sm_50")
    out.append(".address_size 32")
    out.append("")
    out.append(".const .align 8 .b8 context[%d];" % const_bytes)
    out.append(".global .texref t_bvh;")
    out.append("")
    for k in kernels:
        out.append(k.render())
    return "\n".join(out)


# ---- context field offsets (from KERNEL_ABI.md) ----------------------------
class C:
    ATLAS      = 0x000
    WIDTH      = 0x200
    HEIGHT     = 0x204
    FIRSTLINE  = 0x208
    SPP        = 0x20C
    SKYOFF     = 0x210
    SCALE_A    = 0x218
    SCALE_B    = 0x21C
    OUTPUT     = 0x220
    BVH        = 0x224
    MATERIALS  = 0x228
    LIGHTS     = 0x22C
    SKYW       = 0x230
    SKYH       = 0x234
    CAM_DIR    = 0x250
    CAM_UP     = 0x25C
    CAM_RIGHT  = 0x268
    CAM_POS    = 0x274
    LIGHTCOUNT = 0x284
