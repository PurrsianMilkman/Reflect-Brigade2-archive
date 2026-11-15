# Brigade 2 `TracerCUDA` kernel ABI — recovered spec

Everything below was recovered empirically from a running instance on an RTX 3090
(via a string-patched `nvcuda.dll` proxy, `nvcud0.dll`) and cross-validated against
`lib/include/*.h` and `Brigade.lib` disassembly. Items marked **verified** were
confirmed by exact arithmetic or geometric match, not inference.

A replacement module must be **32-bit PTX** (`.address_size 32`) at a virtual arch
the driver can JIT to sm_86 (`.target sm_50` works), dropped in as
`bin/TracerCUDA_sm20.cubin` — the filename is hardcoded (`push $0x14`) and
`cuModuleLoad` accepts PTX regardless of extension.

---

## 1. Module surface

| Symbol | Kind | Notes |
|---|---|---|
| `TracePixelMIS` | `.entry`, no params | the kernel actually launched |
| `TracePixelReflect` | `.entry`, no params | fetched but not launched in this scene |
| `context` | `.const`, **656 bytes** | all state passed through here |
| `t_bvh` | `.texref` | `CU_AD_FORMAT_FLOAT` × 4 channels |

## 2. Launch configuration  **verified**

```
cuFuncSetCacheConfig(TracePixelMIS, CU_FUNC_CACHE_PREFER_L1)
cuFuncSetBlockShape (32, 2, 1)      // 64 threads/block
cuFuncSetSharedSize (0)
cuTexRefSetFormat   (t_bvh, CU_AD_FORMAT_FLOAT, 4)
cuTexRefSetAddress  (t_bvh, bvhBuffer, 14680064)   // byteoffset 0
cuLaunchGrid        (2400, 1)
```

`2400 × 64 = 153600 = 512 × 300` → **one thread per pixel**, no kernel parameters.
Context is created with flags `0xC` (`CU_CTX_MAP_HOST | CU_CTX_SCHED_YIELD`).

## 3. `__constant__ context` — 656 bytes

Offsets are byte offsets. `0xBAADF00D` = never written by the host (unused).

| Offset | Type | Value observed | Meaning |
|---|---|---|---|
| `+0x000` | `void*[128]` | `[0]=0x2B000000`, `[1]=0x2D000000`, rest unwritten | **texture atlas page table**; two 32 MB `byte4` pages |
| `+0x200` | `int` | 512 | render width **verified** |
| `+0x204` | `int` | 300 | render height **verified** |
| `+0x208` | `int` | 0 | first scanline for this worker (multi-GPU split) |
| `+0x20C` | `int` | 4 → 1975 | **rays/samples per pixel** (starts at `samplecount=4`) |
| `+0x210` | `uint` | 36700160 | **sky texel byte offset** into the atlas page array = page 1 + 3145728, exactly where the 2048² sky was uploaded **verified** |
| `+0x214` | `int` | 0 | — |
| `+0x218` | `float` | 1.0 | camera plane scale (semantics TBD) |
| `+0x21C` | `float` | 1.75399 | camera plane scale (semantics TBD) |
| `+0x220` | `float4*` | `0x29C00000` | **output buffer**, host-mapped, `512*300*16 = 2457600 B` **verified** |
| `+0x224` | `float4*` | `0x2A000000` | BVH buffer (same memory as `t_bvh`) |
| `+0x228` | `TracerMaterial*` | `0x2AE00000` | 85 × 80 B = 6800 **verified** |
| `+0x22C` | `TracerLight*` | `0x2AE01C00` | 512 × 64 B = 32768 **verified** |
| `+0x230` | `int` | 2048 | sky width **verified** |
| `+0x234` | `int` | 2048 | sky height **verified** |
| `+0x238`..`+0x24C` | | 0 | — |
| `+0x250` | `float3` | unit | **camera direction** (orthonormal basis) **verified** |
| `+0x25C` | `float3` | unit | **camera up** **verified** |
| `+0x268` | `float3` | unit | **camera right** **verified** |
| `+0x274` | `float3` | `(0, 1.18..2.0, 0)` | **camera position** (y varies with player height) **verified** |
| `+0x280` | `float` | 1.0 | — |
| `+0x284` | `float` | 16 → 20 | **light count** — strongly indicated: exactly 16 emitter triangles exist, and the value steps 16→20 when a lamp activates. Not yet confirmed by a controlled change. |
| `+0x288` | `int` | 0 | — |

Orthonormality was checked across **two runs with different camera orientations**
(all three unit to 1e-6, all pairwise dots < 1e-7):

* **Handedness: `cross(right, up) == +dir`** in both runs — fixes the ray-generation
  convention (could not be determined from a single sample).
* `right.y == 0.0` exactly in both — the camera never rolls, `right` stays horizontal.
* `+0x218` / `+0x21C` were **identical across both orientations**, confirming they are
  orientation-independent projection constants, not view-derived.
  `+0x21C = 1.753994`, which is *not* the render aspect `512/300 = 1.706667`
  (ratio 1.0277) — resolve by changing the window size and re-reading.

## 4. Heap addressing  **verified from `Scene::GetNodeForAddr` disassembly**

```asm
shr edx, 0x11          ; chunk  = addr >> 17
and eax, 0x1FFFF       ; offset = addr & 0x1FFFF
shl eax, 4             ; offset * 16  -> float4 units
add eax, [ecx+edx*4]   ; + chunk base
```

Chunks are 2 MB = 131072 float4 (matches the seven 2 MB `cuMemcpyHtoD` uploads).
Because all chunks live in one contiguous 14 MB allocation, **an address is simply
the global float4 index**, so `tex1Dfetch(t_bvh, addr)` fetches it directly.

## 5. `TracerBVHNode` — 64 B = 4 float4

```
f4+0 : (c0.xmin, c1.xmin, c0.xmax, c1.xmax)
f4+1 : (c0.ymin, c1.ymin, c0.ymax, c1.ymax)
f4+2 : (c0.zmin, c1.zmin, c0.zmax, c1.zmax)
f4+3 : (int child0, int child1, 0, 0)
```

Child encoding — **fully verified against the tree** (a validating traversal from
root 0 reports **28521 inner links OK, 0 failed**):

* `child > 0` → **inner** node at `nodeAddr + child` (**relative**, float4 units).
  Confirmed 46/47 at the top level by exact parent/child AABB match; absolute
  addressing matches 0/47.
* `child < 0` → **leaf**; triangle list begins at the **absolute** address
  * `child` odd  → `(3  - child) >> 1`
  * `child` even → `(12 - child) >> 1`

Iterate a leaf's triangles with stride **9 float4 (144 B)** until the terminator.

> **The list terminator is bit 0 of `woop[0].y`** — i.e. `((int*)woop)[1] & 1`,
> which `scene.h` labels `IsDoubleSided`, *not* the `IsLast` bit on `woop[0].x`.
> This was established empirically: terminating on `woop[0].x` bit 0 reaches only
> 66% of pixels, while `woop[0].y` bit 0 reaches **100%** at 9.0 triangle tests
> per ray. Treat the header's naming here as unreliable.

Validated against an exhaustive no-BVH reference render of the same view:

```
brute force   hits: 240/240 (100.0%)
BVH traversal hits: 240/240 (100.0%)
pixels agreeing on depth: 236/240 (98.3%)
```
Nodes are *not* globally 4-aligned — the heap packs nodes and triangles
interleaved: `[node][tri][tri][node][tri]…`

`SENTINEL 0x76543210` from `settings.h` is the Aila–Laine `EntrypointSentinel`
(traversal-stack marker), not data — it appears nowhere in the buffer.

## 6. `TracerTriangle` — 144 B = 9 float4  **verified** (82.8% of address deltas are exactly 9)

```
+0    float4 woop[3]     // Woop unit-triangle transform
                         //   bit0 of woop[0].x = IsLast   (mask before use)
                         //   bit0 of woop[0].y = DoubleSided
+48   float3 N0, N1, N2  // vertex normals (COMPRESSNORMALS is OFF)
+84   float  U0, U1, U2
+96   float  V0, V1, V2
+108  uint   data        // material index in low 16 bits (0..84); high 16 bits = 0
+112  float3 T, B        // tangent / bitangent (unit)
+136  float  area
+140  Triangle*          // host pointer, meaningless on GPU
```

Scene contains **72,772 triangles**.

### Woop transform — **verified by geometric match**

Recovering vertices (mask the two flag bits first):

```
mtx row0 = woop[1]
mtx row1 = woop[2]
mtx row2 = (woop[0].x & ~1, woop[0].y & ~1, woop[0].z, -woop[0].w)
mtx row3 = (0,0,0,1)
M   = inverse(mtx)
v2  = M.col3 ;  v0 = M.col0 + v2 ;  v1 = M.col1 + v2
```

Decoding the 16 triangles with material index 3 reproduced the 16 emitter
triangles in the light table **exactly**, including vertex order — e.g.
`tri 24382 -> (1.0,0.138,-30.0) (1.0,1.104,-30.0) (0.63,0.211,-30.0)`.

Forward intersection is therefore the standard Aila–Laine form:

```
Oz = woop0.w - dot(org, woop0.xyz);   invDz = 1 / dot(dir, woop0.xyz);  t = Oz * invDz
Ox = woop1.w + dot(org, woop1.xyz);   Dx = dot(dir, woop1.xyz);         u = Ox + t*Dx
Oy = woop2.w + dot(org, woop2.xyz);   Dy = dot(dir, woop2.xyz);         v = Oy + t*Dy
hit if u >= 0 && v >= 0 && u + v <= 1
```

## 7. `TracerMaterial` — 80 B × 85  **verified** (6800 / 80 = 85 exactly)

Layout exactly as declared in `scene.h` (the `// 64 bytes` comment there is stale;
it is 20 dwords = 80 B). Decoded values are all semantically sane: `refractionIndex`
1.33, textured materials carry `1024x1024` + `firstdiffusepixel = 0`, untextured
carry `-1`. Exactly two materials have `flags & EMITTER`, with emissive
`(1000,1000,1000)` and `(200,200,200)` — the lamps.

## 8. `TracerLight` — 64 B × 512  **verified**

`LIGHTTABLESIZE = 512`; `32768 / 64 = 512`. All 512 entries satisfy
`TracerLight::Calculate()` exactly (unit `N`, `area == 0.5*|cross(v1-v0, v2-v0)|`),
**zero mismatches**. It is an *importance table*: 16 distinct emitter triangles
replicated across 512 slots so a uniform random index samples proportional to power.

## 9. Texture atlases

Two 32 MB pages of `byte4` texels, addressed as one virtual array via the page
table at `context+0x000` (page = `off / 33554432`, local = `off % 33554432`).
Individual texture uploads observed at 256², 512², 1024², 2048² × 4 B.
`TracerMaterial::firstdiffusepixel` / `firstnormalpixel` index into it.

---

## Phase 2 milestone 0 — output path proven end-to-end

`src/testpattern.ptx` is a hand-written 32-bit PTX kernel that writes a UV gradient
to `context.output`. Run inside the **real game**, the host-mapped buffer was dumped
and compared against the expected gradient:

```
(511,299) = (0.9980, 0.9967, 0.2000, 1.0000)   expect (0.9980, 0.9967, 0.2000, 1.0000)
mean abs error vs expected gradient: 0.00000 over 2160 samples
```

This confirms, in situ and bit-exact:

* the PTX→JIT→sm_86 path works inside the shipped `TracerCUDA.dll`;
* `context+0x220` is the output buffer, **float4 per pixel, row-major `y*W + x`**,
  components in `0..1`;
* the launch config (64 threads/block × 2400 blocks) maps one thread per pixel with
  index `blockIdx.x*64 + threadIdx.y*32 + threadIdx.x`;
* the zero-copy `cuMemHostAlloc` / `cuMemHostGetDevicePointer` buffer is what
  Brigade reads back.

**The GPU output path is therefore finished and verified.** The screen still shows
per-pixel noise because of the *OpenGL* PBO bug (see `FINDINGS.md` blocker 2) — the
render path is correct, the display path is broken. Frame 1 is drawn before the first
trace completes, and frame 2 faults on the stale PBO pointer before it can present.

Verified output-buffer semantics: the kernel **overwrites** (no accumulation needed
on our side) and `.w = 1.0` is accepted.

## Still open (Phase 2)

* `context+0x218` / `+0x21C` camera plane scales — resolve by resizing the window
  and re-reading, or from `PathTracer::GetRayDir` in `Brigade.lib`.
* `context+0x280` (constant 1.0).
* Output buffer semantics: whether the kernel accumulates across frames or writes
  a fresh average, and what the `.w` channel carries.
* Whether `TracePixelReflect` is needed for the glass-tube maze materials.
