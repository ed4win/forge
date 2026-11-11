# -*- coding: utf-8 -*-
"""把 LZ4 压缩的 NSO0 原位转成未压缩 NSO0（保留 BuildId 与三段 SHA256 哈希）。

    python nso_uncompress.py <in.nso> <out.nso>

头部布局来自 libs/switch-tools/src/elf2nso32.c：
  0x00 magic | 0x04 | 0x08 | 0x0C flags(0x3f=三段都压缩)
  0x10 三段 NsoSegment{FileOff,DstOff,DecompSz,AlignOrTotalSz}
  0x40 BuildId[0x20] | 0x60 CompSz[3] | 0x6C padding | 0x90 Unk4/Unk5
  0xA0 Hashes[3]  (每段未压缩数据的 SHA256)
"""
import struct, sys, hashlib


def lz4d(src, out_size):
    dst = bytearray(); i = 0; n = len(src)
    while i < n and len(dst) < out_size:
        t = src[i]; i += 1
        lit = t >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1; lit += b
                if b != 255:
                    break
        dst += src[i:i + lit]; i += lit
        if i >= n:
            break
        off = src[i] | (src[i + 1] << 8); i += 2
        ml = t & 15
        if ml == 15:
            while True:
                b = src[i]; i += 1; ml += b
                if b != 255:
                    break
        ml += 4
        s = len(dst) - off
        for k in range(ml):
            dst.append(dst[s + k])
    return bytes(dst)


def align(v, a=0x1000):
    return (v + a - 1) & ~(a - 1)


src, dst = sys.argv[1], sys.argv[2]
b = open(src, 'rb').read()
assert b[:4] == b'NSO0'
flags = struct.unpack_from('<I', b, 0x0C)[0]
segs = []
for i in range(3):
    o = 0x10 + i * 16
    file_off, dst_off, decomp_sz, align_or_bss = struct.unpack_from('<IIII', b, o)
    comp_sz = struct.unpack_from('<I', b, 0x60 + i * 4)[0]
    raw = b[file_off:file_off + comp_sz]
    data = lz4d(raw, decomp_sz) if (flags & (1 << i)) else raw
    hdr_hash = b[0xA0 + i * 0x20:0xA0 + (i + 1) * 0x20]
    calc = hashlib.sha256(data).digest()
    print('seg%d file=0x%X dst=0x%X size=%d stored=%d hash_ok=%s' %
          (i, file_off, dst_off, len(data), comp_sz, calc == hdr_hash))
    segs.append((data, dst_off, align_or_bss))

hdr = bytearray(b[:0x100])
struct.pack_into('<I', hdr, 0x0C, flags & ~0x07)   # 清掉三段压缩位 -> 0x38
fo = 0x100
for i, (data, dst_off, align_or_bss) in enumerate(segs):
    o = 0x10 + i * 16
    struct.pack_into('<I', hdr, o + 0, fo)
    struct.pack_into('<I', hdr, o + 4, dst_off)
    struct.pack_into('<I', hdr, o + 8, len(data))
    struct.pack_into('<I', hdr, o + 12, align_or_bss if i == 2 else 0)
    struct.pack_into('<I', hdr, 0x60 + i * 4, len(data))
    fo += len(data)

out = bytes(hdr) + b''.join(s[0] for s in segs)
open(dst, 'wb').write(out)
print('flags 0x%X -> 0x%X, file %d -> %d' % (flags, flags & ~0x07, len(b), len(out)))
