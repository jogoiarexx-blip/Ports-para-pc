from pathlib import Path
import re,sys

root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(".")
data=root/"assets"/"data"
if not data.exists():
    print("SKIP no assets/data")
    sys.exit(0)

attack_total=0
attack_zero_z=0
attack_nonzero_z=[]
throw_heights=[]
toss_heights=[]
shoot_lines=[]
quake=[]
aliases=[]
platforms=[]

for p in data.rglob("*.txt"):
    try:
        lines=p.read_text(errors="ignore").splitlines()
    except Exception:
        continue
    for line_no,raw in enumerate(lines,1):
        line=raw.split("#",1)[0].strip()
        if not line:
            continue
        t=line.split()
        cmd=t[0].lower()
        if re.fullmatch(r"attack\d*",cmd) or cmd in ("burn","shock"):
            attack_total+=1
            if len(t)>10:
                try:
                    z=float(t[10])
                    if z==0:
                        attack_zero_z+=1
                    else:
                        attack_nonzero_z.append((str(p.relative_to(data)),line_no,z))
                except ValueError:
                    pass
        elif cmd=="throwframe" and len(t)>2:
            throw_heights.append(float(t[2]))
        elif cmd=="tossframe" and len(t)>2:
            toss_heights.append(float(t[2]))
        elif cmd=="shootframe":
            shoot_lines.append((str(p.relative_to(data)),line_no,t[1:]))
        elif cmd=="quakeframe":
            quake.append((str(p.relative_to(data)),line_no,t[1:]))
        elif cmd=="alias" and len(t)>1:
            aliases.append(t[1])
        elif cmd=="platform" and len(t)>=9:
            vals=list(map(float,t[1:9]))
            platforms.append(vals)

assert attack_total>=790, attack_total
assert attack_zero_z>=540, attack_zero_z
assert not attack_nonzero_z, attack_nonzero_z
assert sorted(set(throw_heights))==[90.0,101.0,110.0], sorted(set(throw_heights))
assert toss_heights==[50.0,50.0] or sorted(toss_heights)==[50.0,50.0], toss_heights
assert len(shoot_lines)==1 and len(shoot_lines[0][2])==1, shoot_lines
assert len(quake)==3 and all(q[2][:3]==["10","3","-3"] for q in quake), quake
assert len(aliases)==9, aliases
# All eight platform declarations in this module are rectangular; no geometry conversion is needed.
assert len(platforms)==8, len(platforms)
assert all(v[2]==v[3] and v[4]==v[5] for v in platforms), platforms

print(
    "partial_fidelity_assets=OK "
    f"attacks={attack_total} zero_z={attack_zero_z} "
    f"throw={len(throw_heights)} toss={len(toss_heights)} "
    f"quake={len(quake)} aliases={len(aliases)} platforms={len(platforms)}"
)
