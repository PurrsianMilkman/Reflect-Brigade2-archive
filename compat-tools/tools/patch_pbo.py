#!/usr/bin/env python3
"""Fix Reflect.exe's GLByteTexture::UploadData (VA 0x403b30) for modern GL drivers.

Original behaviour (valid only on 2012-era drivers):
    glUnmapBuffer(PBO)            <- unmaps whatever PBO is *currently bound*, not this->buffer
    glBindTexture / glBindBuffer(this->buffer) / glTexSubImage2D
    (Data is never re-mapped -> this->Data dangles; next frame's memcpy faults)

Patched behaviour:
    glBindBuffer(PBO, this->buffer); glUnmapBuffer(PBO);          (cave A, at function entry)
    ... original code ...
    glTexSubImage2D(...); this->Data = glMapBufferARB(PBO, WRITE_ONLY)   (cave B, at function exit)

The caves use absolute GLee function-pointer slots, so the image is pinned to base 0x400000
(DYNAMIC_BASE cleared).  usage: patch_pbo.py <in Reflect.exe> <out Reflect.exe>"""
import sys, struct
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
TEXT_RAW, TEXT_VA = 0x400, 0x401000
off = lambda va: va - TEXT_VA + TEXT_RAW
BIND, UNMAP, MAP, TEXSUB = 0x4b4338, 0x4b4358, 0x4b4828, 0x49e308
call = lambda slot: b'\xff\x15' + struct.pack('<I', slot)
push = lambda imm: b'\x68' + struct.pack('<I', imm)
jmp = lambda frm, to: b'\xe9' + struct.pack('<i', to - (frm + 5))

# --- site A: function entry  `push 88EC; call [glUnmapBuffer]` (11 bytes)
A_SITE, A_CAVE, A_RET = 0x403b30, 0x4055f0, 0x403b3b
assert d[off(A_SITE):off(A_SITE) + 11] == push(0x88ec) + call(UNMAP), 'site A unexpected'
cave_a = (b'\x8b\x46\x04'              # mov eax,[esi+4]      this->buffer
          + b'\x50' + push(0x88ec) + call(BIND)      # glBindBuffer(PBO, buffer)
          + push(0x88ec) + call(UNMAP)                # glUnmapBuffer(PBO)   (now our own buffer)
          )
cave_a += jmp(A_CAVE + len(cave_a), A_RET)
# --- site B: function exit  `call [glTexSubImage2D]; ret` (7 bytes)
B_SITE, B_CAVE = 0x403b77, 0x420750
assert d[off(B_SITE):off(B_SITE) + 7] == call(TEXSUB) + b'\xc3', 'site B unexpected'
cave_b = (call(TEXSUB)
          + push(0x88b9) + push(0x88ec) + call(MAP)   # glMapBufferARB(PBO, GL_WRITE_ONLY_ARB)
          + b'\x89\x46\x10'                           # mov [esi+0x10],eax   this->Data
          + b'\xc3')
for cave, code in ((A_CAVE, cave_a), (B_CAVE, cave_b)):
    assert len(code) <= 32 and all(b == 0xcc for b in d[off(cave):off(cave) + 32]), 'cave %x not free' % cave
    d[off(cave):off(cave) + len(code)] = code
d[off(A_SITE):off(A_SITE) + 11] = jmp(A_SITE, A_CAVE) + b'\xcc' * 6
d[off(B_SITE):off(B_SITE) + 7] = jmp(B_SITE, B_CAVE) + b'\xcc\xcc'
pe = struct.unpack_from('<I', d, 0x3c)[0]
dllch = struct.unpack_from('<H', d, pe + 24 + 0x46)[0]
struct.pack_into('<H', d, pe + 24 + 0x46, dllch & ~0x0040)
open(dst, 'wb').write(d)
print('patched ->', dst, '| caves A %x (%dB) B %x (%dB) | DllCharacteristics %04x -> %04x' % (A_CAVE, len(cave_a), B_CAVE, len(cave_b), dllch, dllch & ~0x40))
