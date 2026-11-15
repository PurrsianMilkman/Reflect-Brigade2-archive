# Reflect on an RTX 3090 — solved (2026-11-08)

`D:\Brigade\reflect_v2\RUNGAME.bat` now runs: the original Brigade 2 path-tracing kernels, translated
from their Fermi (sm_20) machine code to 32-bit PTX, plus a 2-site binary patch of `Reflect.exe`.

## What is installed in `reflect_v2\bin\`
| file | state | original kept as |
|---|---|---|
| `TracerCUDA_sm20.cubin` | now **PTX text** (the DLL hardcodes the name; `cuModuleLoad` accepts PTX) | `TracerCUDA_sm20.cubin.original-fermi` |
| `Reflect.exe` | PBO fix + ASLR pinned to base 0x400000 | `Reflect.exe.original` |

To go back: copy the two `.original*` files over the patched ones.

## How the kernels were recovered (no source exists)
`tools/sass2ptx.py` — cubin -> envydis (`tools/envydis.exe`, built from envytools, gf100 ISA)
-> PTX. ~50 distinct Fermi instruction forms across both kernels (2457 + 2353 instructions).
Re-run: `python -I tools/sass2ptx.py <cubin> <out.ptx>`.

Semantics that mattered (all verified):
* regs `$rN` -> `.b32 %rN`; `.rn` on every float op so ptxas never contracts mul+add.
* `c0[0x4]` add = **local->generic address** (`cvta.local`): the BVH traversal stack lives in local
  memory but is accessed through generic `ld/st`.  `c1[0x100]` = local stack top; `c2[]` = `context`;
  `c16[]` = literal pool (inlined).
* **SIMT stack**: `join` is a real jump to its `joinat` target (a predicated `join nop` skips code!).
  `prebrk/brk`, `precont/cont` resolved statically (innermost enclosing). Verified against a
  real-stack emulator on 10 threads, zero mismatches.
* **Output is packed RGBA8** (one u32/pixel at `out + 4*pixel`) when `context+0x288 == 0`
  (the game's mode); float4/pixel only otherwise. `KERNEL_ABI.md` assumed float4.
* `$clock` is read for RNG seeding (original behaviour, kept).

## Validation
* `tools/sassemu.py` — independent single-thread interpreter of the SASS with true SIMT-stack
  semantics. GPU output == emulator output bit-for-bit on 44/44 threads (fixed clock).
* `tools/runscene.exe` — offline harness (captured BVH/materials/lights, real context): both
  `TracePixelMIS` and `TracePixelReflect` render a recognisable path-traced image.
* In-game: textured chamber, soft GI, stable for 75+ s, launched via RUNGAME.bat.

## The exe patch (`tools/patch_pbo.py`)
`GLByteTexture::UploadData` (VA 0x403b30) unmapped *whichever PBO was bound* and never re-mapped,
so `Data` dangled -> `0xC0000005` on modern drivers.  Cave A: bind own buffer before unmap.
Cave B: after `glTexSubImage2D`, `this->Data = glMapBufferARB(...)`.
Caves use absolute GLee slots, so `DYNAMIC_BASE` is cleared (image loads at 0x400000).

## Not done / caveats
* Not bit-identical to a 2012 GPU: Ampere's `rcp/rsqrt/sin/ex2.approx` differ in the last ulp, and the
  RNG is clock-seeded anyway. Behaviour/look is the same algorithm.
* Performance not benchmarked; adaptive quality in the game adjusts to frame rate as designed.
* Needs the 64-bit NVIDIA driver (32-bit PTX JIT) — verified on driver 610.88.
