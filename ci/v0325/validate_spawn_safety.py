from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
data = root / "assets" / "data"
levels = data / "levels.txt"
if not levels.exists():
    print("SKIP no levels.txt")
    sys.exit(0)

entries = []
zmin = zmax = None
for raw in levels.read_text(errors="ignore").splitlines():
    t = raw.split("#", 1)[0].split()
    if not t:
        continue
    cmd = t[0].lower()
    if cmd == "z":
        zmin = float(t[1])
        zmax = float(t[2])
    elif cmd == "file":
        entries.append((t[1].replace("data/", ""), zmin, zmax))

def gif_width(path):
    with open(path, "rb") as fh:
        hdr = fh.read(10)
    if len(hdr) < 10 or hdr[:3] != b"GIF":
        raise SystemExit(f"bad gif: {path}")
    return int.from_bytes(hdr[6:8], "little")

def parse(rel):
    p = data / rel
    direction = "right"
    walls = []
    panel = None
    for raw in p.read_text(errors="ignore").splitlines():
        t = raw.split("#", 1)[0].split()
        if not t:
            continue
        cmd = t[0].lower()
        if cmd == "direction":
            direction = t[1].lower()
        elif cmd == "wall":
            walls.append(list(map(float, t[1:9])))
        elif cmd == "panel":
            panel = t[1].replace("data/", "")
    return direction, walls, panel

def inside(w, x, z):
    wx, wz, ul, ll, ur, lr, depth, height = w
    depth = abs(depth)
    top = wz - depth
    if z < top or z > wz:
        return False
    q = (z - top) / depth if depth else 1.0
    left = wx + ul + (ll - ul) * q
    right = wx + ur + (lr - ur) * q
    if left > right:
        left, right = right, left
    return left <= x <= right

def safe_stage(idx, view, pi):
    rel, lo, hi = entries[idx - 1]
    direction, walls, panel = parse(rel)
    ww = gif_width(data / panel)
    reverse = direction in ("left", "leftright")
    camera = max(0, ww - view) if reverse else 0
    x = camera + (250 - pi * 24 if reverse else 70 + pi * 24)
    z = (lo + hi) / 2 + (pi - 1) * 10

    def blocked(xx, zz):
        return any(inside(w, xx, zz) for w in walls)

    was = blocked(x, z)
    if not was:
        return was, False, (x, z)

    depth_candidates = [z]
    for step in range(1, 128):
        added = False
        z_hi = z + 3 * step
        z_lo = z - 3 * step
        if z_hi <= hi:
            depth_candidates.append(z_hi)
            added = True
        if z_lo >= lo:
            depth_candidates.append(z_lo)
            added = True
        if not added:
            break
    if lo not in depth_candidates:
        depth_candidates.append(lo)
    if hi not in depth_candidates:
        depth_candidates.append(hi)

    out = (x, z)
    x_steps = max(1, int(max(32, view) / 4))
    for step in range(x_steps + 1):
        xx = x + (-4 * step if reverse else 4 * step)
        if xx < 8 or xx > ww - 8:
            continue
        found = False
        for zz in depth_candidates:
            if not blocked(xx, zz):
                out = (xx, zz)
                found = True
                break
        if found:
            break
    return was, blocked(*out), out

expected = {(15, 320), (17, 320), (19, 320), (19, 426)}
found = set()

for idx in range(1, len(entries) + 1):
    for view in (320, 426):
        for pi in range(3):
            was, still, out = safe_stage(idx, view, pi)
            if was:
                found.add((idx, view))
                if still:
                    raise SystemExit(
                        f"unsafe after search stage={idx} view={view} p={pi+1} out={out}"
                    )

if not expected.issubset(found):
    raise SystemExit(
        f"expected collisions not reproduced: expected={sorted(expected)} found={sorted(found)}"
    )

print("player_spawn_safety=OK collisions=" + str(sorted(found)))
