# Reflect (Brigade 2) - archived running copy

A school project (NHTV/IGAD, Breda; Brigade 2 path tracer by Jacco Bikker and Jeroen van Schijndel), archived
**as it currently runs** on a modern NVIDIA GPU (tested: RTX 3090, driver 610, Windows 11). Run `RUNGAME.bat`.

Two things differ from the 2012 package:
* `bin/TracerCUDA_sm20.cubin` is now **32-bit PTX text** translated from the original Fermi cubin
  (kept as `bin/TracerCUDA_sm20.cubin.original-fermi`). The DLL hardcodes the file name; the driver accepts PTX.
* `bin/Reflect.exe` is patched (pixel-buffer re-map + fixed image base); the original is `bin/Reflect.exe.original`.

`compat-tools/` has the translator, emulator, patcher and write-up (see `compat-tools/docs/SOLUTION.md`).
The original `readme.txt` is unchanged. Third-party binaries (FMOD, Assimp, FreeImage, Bullet) keep their own licences.
