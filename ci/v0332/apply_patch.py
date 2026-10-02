from pathlib import Path
import os, base64, zlib, subprocess

src=Path(os.environ["FFX_SRC"])
workspace=Path(os.environ["GITHUB_WORKSPACE"])
temp=Path(os.environ["RUNNER_TEMP"])
for rel in ["CMakeLists.txt","src/main_win.cpp","src/OpenBorSemantics.h","src/Game.h","src/Game.cpp"]:
    p=src/rel
    text=p.read_text(encoding="utf-8-sig").replace("\r\n","\n")
    p.write_text(text,encoding="utf-8")

payload=workspace/"ci/v0332/ffx-v0332.patch.zlib.b64"
patch=temp/"ffx-v0332.patch"
patch.write_bytes(zlib.decompress(base64.b64decode(payload.read_text().strip())))
print("decoded v0.3.32 patch",patch.stat().st_size)
subprocess.check_call(["git","-C",str(src),"apply","--check",str(patch)])
subprocess.check_call(["git","-C",str(src),"apply",str(patch)])
print("v0.3.32 PAK fidelity patch applied")
