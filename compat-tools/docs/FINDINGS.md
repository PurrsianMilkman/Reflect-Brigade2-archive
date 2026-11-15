# Reflect / Brigade 2 — RTX 3090 compatibility analysis

Target: NVIDIA GeForce RTX 3090 (Ampere, **sm_86**), driver 610.88 / CUDA 13.0,
Ryzen 7 9800X3D (16 logical CPUs), Windows 11 26100.

All binaries in the package are **32-bit x86** (`Reflect.exe`, `TracerCUDA.dll`,
`Assimp32.dll`, `FreeImage.dll`). Brigade.lib is a static MSVC 9.0 (VS2008) lib.

---

## Blocker 1 — CUDA: the shipped kernels are Fermi-only machine code

`bin/TracerCUDA_sm20.cubin` is an **ELF32, EM_CUDA, e_flags=0x140114 → sm_20**
container holding pure Fermi SASS. Its complete contents:

| Symbol | Kind | Size |
|---|---|---|
| `TracePixelMIS` | `__global__` kernel | 19656 B SASS |
| `TracePixelReflect` | `__global__` kernel | 18824 B SASS |
| `context` | `__constant__` struct (`.nv.constant2`) | **656 bytes** |
| `t_bvh` | texture reference | bound at runtime |

There is **no embedded PTX** (no `.ptx` section, no `.version`/`.target` strings),
so there is nothing for the driver to JIT. Fermi SASS cannot execute on Ampere.

Verified directly against the 3090:

```
cuModuleLoad("TracerCUDA_sm20.cubin") -> 209 (CUDA_ERROR_NO_BINARY_FOR_GPU)
```

`TracerCUDA.dll` hardcodes the SM level — it never queries the GPU:

```asm
10001604: push $0x14            ; 20, hardcoded
10001606: push $0x10007544      ; "TracerCUDA"
1000160f: push $0x100074e8      ; "%s_sm%i.cubin"
1000161d: call *0x1000611c      ; sprintf -> "TracerCUDA_sm20.cubin"
```

Consistently, its import list contains **no** compute-capability query
(`cuDeviceGetAttribute` / `cuDeviceComputeCapability` are absent).

The DLL does contain a full runtime `nvcc` invocation path
(`--machine 32 ... -arch sm_%i -cubin`), i.e. Brigade 2 was designed to compile
`TracerCUDA.cu` on demand — **but that `.cu` is not in this package**, and
Brigade 2's tracer is closed source (sold to OTOY; only snippets were ever public).

### Downstream symptom: infinite hang, not an error message

With no loadable module the CUDA tracer registers **zero enabled workers**.
`PathTracer::RenderBegin` then spins forever in its scanline load-balancer
(`Reflect.exe+0x427b7`, confirmed by thread sampling — one core pinned at 100%):

```asm
4427d3: cmpb $0x0,0x8(%eax)   ; worker->enabled ?
4427d7: je   4427e3           ; not enabled -> contribute nothing
4427d9: add  %ebx,0x24(%eax)  ; enabled -> assign lines
4427dc: add  %ebx,%edx        ; total += lines
4427f8: jl   4427b7           ; loop while total < target  <-- never advances
```

That is why the game sits on the "Brigade 2" splash screen forever.

---

## Blocker 2 — OpenGL: PBO unmapped and never re-mapped (independent bug)

Masked until now because Blocker 1 stopped execution before the second frame.

`Reflect/Graphics.cpp` — `GLByteTexture::UploadData()` calls `glUnmapBuffer()`
but never re-maps, leaving `GLByteTexture::Data` dangling:

```cpp
void GLByteTexture::UploadData() {
    glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB );   // Data is now invalid
    glBindTexture( GL_TEXTURE_2D, texture );
    glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, buffer );
    glTexSubImage2D( ... );
    // no re-map -> Data stays stale
}
```

Frame 2 then writes through the stale pointer at
[`Reflect/main.cpp:253`](../reflect_v2/Reflect/main.cpp#L253):

```cpp
memcpy(RenderBuffer->Data, finalimage.GetBuffer(),
       RenderResolution.X*RenderResolution.Y*sizeof(byte4));
```

Observed fault and the destination's actual state:

```
FATAL 0xC0000005 at msvcr90.dll+0x24651   (memcpy)
    WROTE address 0x0DC37000
    probe 0DC37000 -> allocBase=0D0D0000 size=614400 RESERVE prot=0x0
```

`614400 == 512*300*4` — exactly the render buffer — in state **MEM_RESERVE /
PAGE_NOACCESS**. 2012-era drivers left unmapped PBO pages committed, so the stale
write silently worked; modern drivers decommit on unmap, so it faults.

**Fix** (re-map after upload):

```cpp
void GLByteTexture::UploadData() {
    glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER_ARB );
    glBindTexture( GL_TEXTURE_2D, texture );
    glBindBuffer( GL_PIXEL_UNPACK_BUFFER_ARB, buffer );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, Size.X, Size.Y,
                     GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    Data = (unsigned char*)glMapBufferARB( GL_PIXEL_UNPACK_BUFFER_ARB,
                                           GL_WRITE_ONLY_ARB );   // <-- add
}
```

Requires rebuilding `Reflect.exe` against the VS2008-era `Brigade.lib`.

---

## The delivery mechanism works — proven on this GPU

A 32-bit process on the current driver **can** load 32-bit PTX, JIT it to sm_86,
and launch it through the same deprecated API this engine uses
(`cuFuncSetBlockShape` / `cuParamSetv` / `cuLaunchGrid`). All 23 legacy driver
entry points `TracerCUDA.dll` needs are still exported by `nvcuda.dll`
(both System32 and SysWOW64).

`cuModuleLoad` accepts PTX regardless of file extension, so a replacement can
simply be dropped in as `TracerCUDA_sm20.cubin`.

`TracerCUDA_sm20.stub.ptx` here is an ABI-exact stub. Against the 3090:

```
cuModuleLoad("stub.ptx")        -> 0 (CUDA_SUCCESS)
  GetFunction(TracePixelMIS)     -> 0
  GetFunction(TracePixelReflect) -> 0
  GetGlobal(context)             -> 0  size=656      <-- exact match
  GetTexRef(t_bvh)               -> 0
```

With the stub in place the engine gets **past** Blocker 1: the module loads,
workers enable, the splash clears and the render loop runs — it then dies on
Blocker 2. The stub's kernels are empty, so it renders nothing; it is a
compatibility proof, not a renderer.

---

## What full restoration requires

Write `TracePixelMIS` and `TracePixelReflect` from scratch and compile to 32-bit
PTX (`-m32 -ptx -arch=compute_50`, CUDA ≤ 10.2 — CUDA 11+ dropped `-m32`;
the driver JITs compute_50 → sm_86).

Known and in your favour:
* GPU-side data structures are fully declared in `lib/include/scene.h`
  (`TracerMaterial`, `TracerTriangle`, `TracerBVHNode`, `TracerLight`) and `BVH.h`.
* The ABI surface is tiny: 2 parameterless kernels, one 656-byte `__constant__`
  struct, one texture reference.
* Launch configuration is chosen by the DLL, not the kernel.

Unknown and must be reverse-engineered:
* The internal layout of the 656-byte `context` struct (buffer device pointers,
  camera basis, resolution, counts, sample state).
  Recoverable by proxying `nvcuda.dll` and dumping the `cuMemcpyHtoD` payload —
  patch the string `"nvcuda.dll"` at file offset `0x6908` in `TracerCUDA.dll` to an
  equal-length name (e.g. `"nvcud0.dll"`) so the proxy actually intercepts;
  loading it under the real name fails because the OpenGL driver has already
  registered `nvcuda.dll` by base name.
* The BVH memory layout as packed into the `t_bvh` texture.
* The output buffer format written by the kernels.

This is a full GPU path tracer reimplementation (BVH traversal, material
evaluation, MIS light sampling, texture fetch), not a patch.

---

## Ruled out

* **Recompiling the shipped kernels** — no `.cu`, closed source.
* **JIT from the cubin** — contains no PTX.
* **Translating Fermi SASS → Ampere** — no tooling; undocumented ISA.
* **A CPU fallback** — `Brigade.lib` contains only the abstract `Tracer` base and
  `PathTracer` orchestration; every tracer implementation lives in an external
  DLL, and only the CUDA one shipped.
* **Driver / config / compatibility-mode tweaks** — the failure is compiled-in
  machine code for a GPU architecture that no longer exists.

## Tools (`tools/`, 32-bit, build with i686-w64-mingw32-gcc)

* `probe.c` — loads the shipped cubin and a 32-bit PTX on the real driver
* `probe2.c` — validates a replacement module against the required ABI
* `sampler.c` — thread/EIP sampler with EBP backtrace (found the spin loop)
* `runwatch.c` — debugger: freezes only the faulting thread, dumps context,
  stack and the destination's `VirtualQueryEx` state (found the PBO bug)
* `shim2.c` — logging `nvcuda.dll` proxy (needs the DLL string patch above)
