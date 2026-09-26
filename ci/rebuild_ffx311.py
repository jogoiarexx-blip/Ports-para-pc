from pathlib import Path
import hashlib
import lzma
import struct
import sys

OLD_SHA = "3761b33b32381b2c3b18b74ee97272cc040287d9dbcf920d630d79596bdb8bb6"
PATCH_SHA = "eed29c066de3f391600c7999abd5c68519d37a44c62831c1cfe0ef239c9a6426"
NEW_SHA = "d2d7e503b1c9c5689fd615afd89151117919e209908aa77355b9fff2ce1f3515"
NEW_SIZE = 650240

if len(sys.argv) != 4:
    raise SystemExit("usage: rebuild_ffx311.py old.exe delta.xz new.exe")

old_path, patch_path, out_path = map(Path, sys.argv[1:])
old = old_path.read_bytes()
patch_xz = patch_path.read_bytes()

if hashlib.sha256(old).hexdigest() != OLD_SHA:
    raise SystemExit("v0.3.10 base EXE hash mismatch")
if hashlib.sha256(patch_xz).hexdigest() != PATCH_SHA:
    raise SystemExit("v0.3.11 delta hash mismatch")

raw = lzma.decompress(patch_xz)
if raw[:5] != b"FFXD1":
    raise SystemExit("invalid delta magic")

p = 5
out = bytearray()
while True:
    if p >= len(raw):
        raise SystemExit("truncated delta")
    op = raw[p:p+1]
    p += 1
    if op == b"E":
        break
    if op == b"C":
        if p + 8 > len(raw):
            raise SystemExit("truncated copy record")
        off, n = struct.unpack_from("<II", raw, p)
        p += 8
        if off + n > len(old):
            raise SystemExit("copy record outside base EXE")
        out += old[off:off+n]
    elif op == b"D":
        if p + 4 > len(raw):
            raise SystemExit("truncated data record")
        n = struct.unpack_from("<I", raw, p)[0]
        p += 4
        if p + n > len(raw):
            raise SystemExit("truncated literal data")
        out += raw[p:p+n]
        p += n
    else:
        raise SystemExit(f"unknown delta opcode: {op!r}")

new = bytes(out)
if len(new) != NEW_SIZE:
    raise SystemExit(f"unexpected rebuilt EXE size: {len(new)}")
sha = hashlib.sha256(new).hexdigest()
if sha != NEW_SHA:
    raise SystemExit(f"rebuilt EXE hash mismatch: {sha}")

out_path.write_bytes(new)
print(f"OK size={len(new)} sha256={sha}")
