#!/usr/bin/env python3
"""Fill 2D TGA silhouettes from GoldSrc w_*.mdl triangle meshes (not vertex dots)."""
from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "data" / "ui-overrides" / "cstrike" / "resource" / "buy"
WIDTH, HEIGHT = 256, 128

WEAPONS = [
    "p228", "glock18", "scout", "hegrenade", "xm1014", "mac10", "aug",
    "smokegrenade", "elite", "fiveseven", "ump45", "sg550", "galil", "famas",
    "usp", "awp", "mp5", "m249", "m3", "m4a1", "tmp", "g3sg1", "flashbang",
    "deagle", "sg552", "ak47", "p90", "molotov", "incgrenade", "kevlar",
    "assault", "thighpack",
]


def read_i32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<i", buf, off)[0]


def load_tris(path: Path) -> list[tuple[tuple[float, float, float], ...]]:
    data = path.read_bytes()
    if len(data) < 244 or data[0:4] != b"IDST":
        return []
    num_body = read_i32(data, 204)
    body_off = read_i32(data, 208)
    tris: list[tuple[tuple[float, float, float], ...]] = []
    for b in range(max(0, num_body)):
        bo = body_off + b * 76
        if bo + 76 > len(data):
            break
        nmodels = read_i32(data, bo + 64)
        modelindex = read_i32(data, bo + 72)
        for m in range(max(0, nmodels)):
            mo = modelindex + m * 112
            if mo + 112 > len(data):
                break
            nmesh = read_i32(data, mo + 72)
            meshindex = read_i32(data, mo + 76)
            nverts = read_i32(data, mo + 80)
            vertindex = read_i32(data, mo + 88)
            verts: list[tuple[float, float, float]] = []
            for v in range(max(0, nverts)):
                vo = vertindex + v * 12
                if vo + 12 > len(data):
                    verts.append((0.0, 0.0, 0.0))
                    continue
                x, y, z = struct.unpack_from("<fff", data, vo)
                verts.append((x, y, z))
            for mesh in range(max(0, nmesh)):
                me = meshindex + mesh * 20
                if me + 20 > len(data):
                    break
                triindex = read_i32(data, me + 4)
                off = triindex
                while off + 2 <= len(data):
                    cmd = struct.unpack_from("<h", data, off)[0]
                    off += 2
                    if cmd == 0:
                        break
                    fan = cmd < 0
                    count = -cmd if fan else cmd
                    idx: list[int] = []
                    ok = True
                    for _ in range(count):
                        if off + 8 > len(data):
                            ok = False
                            break
                        vi = struct.unpack_from("<h", data, off)[0]
                        off += 8
                        if vi < 0 or vi >= len(verts):
                            ok = False
                            break
                        idx.append(vi)
                    if not ok or len(idx) < 3:
                        break
                    if fan:
                        origin = idx[0]
                        for i in range(1, len(idx) - 1):
                            tris.append((verts[origin], verts[idx[i]], verts[idx[i + 1]]))
                    else:
                        for i in range(len(idx) - 2):
                            if i % 2 == 0:
                                tris.append((verts[idx[i]], verts[idx[i + 1]], verts[idx[i + 2]]))
                            else:
                                tris.append((verts[idx[i]], verts[idx[i + 2]], verts[idx[i + 1]]))
    return tris


def fill_triangle(cover: list[int], a: tuple[float, float], b: tuple[float, float], c: tuple[float, float]) -> None:
    ax, ay = a
    bx, by = b
    cx, cy = c
    minx = max(0, int(min(ax, bx, cx)))
    maxx = min(WIDTH - 1, int(max(ax, bx, cx)))
    miny = max(0, int(min(ay, by, cy)))
    maxy = min(HEIGHT - 1, int(max(ay, by, cy)))
    area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
    if area == 0:
        return
    for y in range(miny, maxy + 1):
        for x in range(minx, maxx + 1):
            w0 = (bx - ax) * (y - ay) - (by - ay) * (x - ax)
            w1 = (cx - bx) * (y - by) - (cy - by) * (x - bx)
            w2 = (ax - cx) * (y - cy) - (ay - cy) * (x - cx)
            if (w0 >= 0 and w1 >= 0 and w2 >= 0) or (w0 <= 0 and w1 <= 0 and w2 <= 0):
                cover[y * WIDTH + x] = 1


def raster(tris: list[tuple[tuple[float, float, float], ...]]) -> bytearray:
    img = bytearray(WIDTH * HEIGHT * 4)
    if not tris:
        return img
    pts = [v for tri in tris for v in tri if v[0] == v[0] and v[1] == v[1] and v[2] == v[2]]
    if not pts:
        return img
    mins = [min(p[i] for p in pts) for i in range(3)]
    maxs = [max(p[i] for p in pts) for i in range(3)]
    spans = [max(maxs[i] - mins[i], 1.0) for i in range(3)]
    axis_u, axis_v = sorted(range(3), key=lambda i: spans[i], reverse=True)[:2]
    pad = 10.0
    scale = min((WIDTH - 2 * pad) / spans[axis_u], (HEIGHT - 2 * pad) / spans[axis_v])
    cu = (mins[axis_u] + maxs[axis_u]) * 0.5
    cv = (mins[axis_v] + maxs[axis_v]) * 0.5

    def proj(v: tuple[float, float, float]) -> tuple[float, float]:
        return (v[axis_u] - cu) * scale + WIDTH * 0.5, -(v[axis_v] - cv) * scale + HEIGHT * 0.5

    cover = [0] * (WIDTH * HEIGHT)
    for tri in tris:
        fill_triangle(cover, proj(tri[0]), proj(tri[1]), proj(tri[2]))
    dilated = cover[:]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if cover[y * WIDTH + x]:
                continue
            hit = False
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < WIDTH and 0 <= yy < HEIGHT and cover[yy * WIDTH + xx]:
                        hit = True
                        break
                if hit:
                    break
            if hit:
                dilated[y * WIDTH + x] = 1
    for i, c in enumerate(dilated):
        if c:
            o = i * 4
            img[o : o + 4] = b"\xff\xff\xff\xff"
    return img


def write_tga(path: Path, pixels: bytes) -> None:
    header = bytearray(18)
    header[2] = 2
    header[12:14] = struct.pack("<H", WIDTH)
    header[14:16] = struct.pack("<H", HEIGHT)
    header[16] = 32
    header[17] = 8
    rows = bytearray()
    for y in range(HEIGHT - 1, -1, -1):
        rows.extend(pixels[y * WIDTH * 4 : (y + 1) * WIDTH * 4])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + rows)


def model_dirs() -> list[Path]:
    dirs: list[Path] = []
    env = os.environ.get("XASH3D_RODIR") or os.environ.get("CSRETRO_GAMEDATA")
    if env:
        dirs.append(Path(env) / "cstrike" / "models")
    dirs.append(ROOT / "gamedata" / "cstrike" / "models")
    return dirs


def main() -> int:
    models = None
    for d in model_dirs():
        if (d / "w_ak47.mdl").is_file():
            models = d
            break
    if models is None:
        print("build_buy_silhouettes: no w_*.mdl (skip)", file=sys.stderr)
        return 0
    n = 0
    for stem in WEAPONS:
        src = models / f"w_{stem}.mdl"
        if not src.is_file():
            continue
        write_tga(OUT / f"w_{stem}.tga", raster(load_tris(src)))
        n += 1
    print(f"build_buy_silhouettes: {n} TGA → {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
