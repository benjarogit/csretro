#!/usr/bin/env python3
"""Build the native Incendiary view/world models and optional Molotov experiment.

Molotov retargeting is experimental and disabled when
CSRETRO_SKIP_MOLOTOV_RETARGET=1. The intact 2008 model is the runtime default;
mixing CS 1.6 HE hand vertices into its unrelated skeleton has produced
corrupted polygons in game.

Incendiary: CS 1.6 smoke hands/anims. The Fire-Pack p_inc can (Cylinder01)
is fitted into Bone02, the same socket as v_smokegrenade / v_hegrenade.

World incendiary: smoke toss sequences, canister texture on f_body.

Tools are not vendored. CSRETRO_DECOMPMDL / CSRETRO_STUDIOMDL.
"""
from __future__ import annotations

import math
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HAND_DROP = {"hand.bmp", "thumb.bmp"}
Q_PI = math.pi


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def gamedata_cstrike() -> Path:
    root = repo_root()
    gd = Path(os.environ.get("CSRETRO_GAMEDATA", root / "gamedata"))
    return gd / "cstrike"


def which_tool(env_name: str, fallbacks: list[Path]) -> Path | None:
    env = os.environ.get(env_name)
    if env:
        p = Path(env)
        if p.is_file() and os.access(p, os.X_OK):
            return p
    for p in fallbacks:
        if p.is_file() and os.access(p, os.X_OK):
            return p
    names = {"CSRETRO_DECOMPMDL": "decompmdl", "CSRETRO_STUDIOMDL": "pxstudiomdl"}
    found = shutil.which(names.get(env_name, ""))
    return Path(found) if found else None


def run(cmd: list[str], cwd: Path | None = None) -> None:
    subprocess.run(cmd, cwd=cwd, check=True)


def decompile(decomp: Path, mdl: Path, dest: Path) -> Path:
    dest.mkdir(parents=True, exist_ok=True)
    run([str(decomp), str(mdl), "."], cwd=dest)
    nested = dest / mdl.stem
    if (nested / f"{mdl.stem}.qc").is_file():
        return nested
    qcs = list(dest.glob("**/*.qc"))
    if not qcs:
        raise SystemExit(f"decompmdl wrote no QC for {mdl}")
    return qcs[0].parent


def copy_textures(src_maps: Path, dst_maps: Path, names: list[str]) -> None:
    dst_maps.mkdir(parents=True, exist_ok=True)
    for name in names:
        matches = [p for p in src_maps.iterdir() if p.name.lower() == name.lower()]
        if not matches:
            raise SystemExit(f"texture missing: {name} in {src_maps}")
        shutil.copy2(matches[0], dst_maps / matches[0].name.lower())


def palettize_resize(src: Path, dst: Path, size: tuple[int, int]) -> None:
    from PIL import Image

    im = Image.open(src)
    if im.mode != "RGB":
        im = im.convert("RGB")
    im = im.resize(size, Image.Resampling.LANCZOS)
    im = im.convert("P", palette=Image.ADAPTIVE, colors=256)
    dst.parent.mkdir(parents=True, exist_ok=True)
    im.save(dst, format="BMP")


def replace_first_texture(maps_dir: Path, inc_tex: Path, candidates: list[str], size: tuple[int, int]) -> str:
    for name in candidates:
        matches = [p for p in maps_dir.iterdir() if p.name.lower() == name.lower()]
        if matches:
            palettize_resize(inc_tex, matches[0], size)
            return matches[0].name
    raise SystemExit(f"no body texture in {maps_dir}, tried {candidates}")


def compile_qc(studiomdl: Path, qc: Path, expected_name: str) -> Path:
    log = qc.with_suffix(".compile.log")
    with log.open("w", encoding="utf-8") as fh:
        subprocess.run([str(studiomdl), qc.name], cwd=qc.parent, check=True, stdout=fh, stderr=subprocess.STDOUT)
    built = qc.parent / expected_name
    if not built.is_file():
        raise SystemExit(
            f"{studiomdl} did not write {built}\n{log.read_text(encoding='utf-8', errors='replace')[-2000:]}"
        )
    return built


# --- GoldSrc studiomdl bone math (studiomdl.c Build_Reference) ---


def angle_matrix(angles: list[float]) -> list[list[float]]:
    sy, cy = math.sin(angles[2] * Q_PI * 2 / 360), math.cos(angles[2] * Q_PI * 2 / 360)
    sp, cp = math.sin(angles[1] * Q_PI * 2 / 360), math.cos(angles[1] * Q_PI * 2 / 360)
    sr, cr = math.sin(angles[0] * Q_PI * 2 / 360), math.cos(angles[0] * Q_PI * 2 / 360)
    m = [[0.0] * 4 for _ in range(3)]
    m[0][0] = cp * cy
    m[1][0] = cp * sy
    m[2][0] = -sp
    m[0][1] = sr * sp * cy + cr * -sy
    m[1][1] = sr * sp * sy + cr * cy
    m[2][1] = sr * cp
    m[0][2] = cr * sp * cy + -sr * -sy
    m[1][2] = cr * sp * sy + -sr * cy
    m[2][2] = cr * cp
    return m


def angle_imatrix(angles: list[float]) -> list[list[float]]:
    sy, cy = math.sin(angles[2] * Q_PI * 2 / 360), math.cos(angles[2] * Q_PI * 2 / 360)
    sp, cp = math.sin(angles[1] * Q_PI * 2 / 360), math.cos(angles[1] * Q_PI * 2 / 360)
    sr, cr = math.sin(angles[0] * Q_PI * 2 / 360), math.cos(angles[0] * Q_PI * 2 / 360)
    m = [[0.0] * 4 for _ in range(3)]
    m[0][0] = cp * cy
    m[0][1] = cp * sy
    m[0][2] = -sp
    m[1][0] = sr * sp * cy + cr * -sy
    m[1][1] = sr * sp * sy + cr * cy
    m[1][2] = sr * cp
    m[2][0] = cr * sp * cy + -sr * -sy
    m[2][1] = cr * sp * sy + -sr * cy
    m[2][2] = cr * cp
    return m


def concat(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    out = [[0.0] * 4 for _ in range(3)]
    for i in range(3):
        for j in range(3):
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]
        out[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3]
    return out


def vxf(v: list[float], m: list[list[float]]) -> list[float]:
    return [
        v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2] + m[0][3],
        v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2] + m[1][3],
        v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2] + m[2][3],
    ]


class Smd:
    def __init__(self) -> None:
        self.nodes: list[tuple[int, str, int]] = []
        self.bones: dict[int, tuple[tuple[float, float, float], tuple[float, float, float]]] = {}
        self.tris: list[tuple[str, list[tuple]]] = []
        self.header: list[str] = []

    @property
    def name_to_idx(self) -> dict[str, int]:
        return {name: idx for idx, name, _ in self.nodes}


def parse_smd(path: Path) -> Smd:
    smd = Smd()
    lines = path.read_text(encoding="latin-1").splitlines()
    mode = None
    i = 0
    while i < len(lines):
        raw = lines[i]
        s = raw.strip()
        if mode is None and s not in ("nodes", "skeleton", "triangles"):
            smd.header.append(raw)
            i += 1
            continue
        if s == "nodes":
            mode = "nodes"
            i += 1
            continue
        if s == "skeleton":
            mode = "skel"
            i += 1
            continue
        if s == "triangles":
            mode = "tri"
            i += 1
            continue
        if s == "end":
            mode = None
            i += 1
            continue
        if mode == "nodes":
            q = s.split('"')
            smd.nodes.append((int(q[0].split()[0]), q[1], int(q[2].split()[0])))
        elif mode == "skel" and not s.startswith("time"):
            p = s.split()
            smd.bones[int(p[0])] = (
                (float(p[1]), float(p[2]), float(p[3])),
                (float(p[4]), float(p[5]), float(p[6])),
            )
        elif mode == "tri":
            mat = s
            verts = []
            for _ in range(3):
                i += 1
                p = lines[i].split()
                verts.append(
                    (
                        int(p[0]),
                        float(p[1]),
                        float(p[2]),
                        float(p[3]),
                        float(p[4]),
                        float(p[5]),
                        float(p[6]),
                        float(p[7]),
                        float(p[8]),
                    )
                )
            smd.tris.append((mat, verts))
        i += 1
    return smd


def build_fixup(smd: Smd) -> tuple[dict, dict, dict]:
    m_out: dict[int, list[list[float]]] = {}
    im_out: dict[int, list[list[float]]] = {}
    world: dict[int, list[float]] = {}
    for idx, _name, parent in smd.nodes:
        pos, rot = smd.bones[idx]
        ang = [rot[0] * 180 / Q_PI, rot[1] * 180 / Q_PI, rot[2] * 180 / Q_PI]
        if parent == -1:
            m_out[idx] = angle_matrix(ang)
            im_out[idx] = angle_imatrix(ang)
            world[idx] = [pos[0], pos[1], pos[2]]
        else:
            m_out[idx] = concat(m_out[parent], angle_matrix(ang))
            im_out[idx] = concat(angle_imatrix(ang), im_out[parent])
            p = vxf(list(pos), m_out[parent])
            world[idx] = [p[0] + world[parent][0], p[1] + world[parent][1], p[2] + world[parent][2]]
    return m_out, im_out, world


def to_local(p: list[float], world: list[float], im: list[list[float]]) -> list[float]:
    tmp = [p[0] - world[0], p[1] - world[1], p[2] - world[2]]
    return vxf(tmp, im)


def to_world(local: list[float], world: list[float], m: list[list[float]]) -> list[float]:
    p = vxf(local, m)
    return [p[0] + world[0], p[1] + world[1], p[2] + world[2]]


def write_smd(path: Path, nodes: list, bones: dict, tris: list) -> None:
    lines = ["version 1", "nodes"]
    for idx, name, parent in nodes:
        lines.append(f"  {idx} \"{name}\" {parent}")
    lines.append("end")
    lines.append("skeleton")
    lines.append("  time 0")
    for idx, _name, _parent in nodes:
        pos, rot = bones[idx]
        lines.append(
            f"    {idx} {pos[0]:.6f} {pos[1]:.6f} {pos[2]:.6f} {rot[0]:.6f} {rot[1]:.6f} {rot[2]:.6f}"
        )
    lines.append("end")
    lines.append("triangles")
    for mat, verts in tris:
        lines.append(mat)
        for v in verts:
            lines.append(
                f"    {v[0]} {v[1]:.4f} {v[2]:.4f} {v[3]:.4f} {v[4]:.4f} {v[5]:.4f} {v[6]:.4f} {v[7]:.4f} {v[8]:.4f}"
            )
    lines.append("end")
    path.write_text("\n".join(lines) + "\n", encoding="latin-1")


def filter_bottle_tris(smd: Smd) -> list:
    drop = {n.lower() for n in HAND_DROP}
    return [(mat, verts) for mat, verts in smd.tris if mat.lower() not in drop]


def retarget_hand(he: Smd, dest_nodes: list, dest_bones: dict, dest_fixup) -> list:
    """HE hand verts → dest (molotov) model space via matching bone names."""
    he_m, he_im, he_w = build_fixup(he)
    dest_m, dest_im, dest_w = dest_fixup
    he_names = {idx: name for idx, name, _ in he.nodes}
    dest_idx = {name: idx for idx, name, _ in dest_nodes}
    fallback = dest_idx["Bone_Righthand"]
    out = []
    for mat, verts in he.tris:
        nv = []
        for v in verts:
            name = he_names[v[0]]
            di = dest_idx.get(name, fallback)
            local = to_local([v[1], v[2], v[3]], he_w[v[0]], he_im[v[0]])
            world = to_world(local, dest_w[di], dest_m[di])
            nlocal = to_local([v[4], v[5], v[6]], [0.0, 0.0, 0.0], he_im[v[0]])
            nworld = vxf(nlocal, dest_m[di])
            length = math.sqrt(nworld[0] ** 2 + nworld[1] ** 2 + nworld[2] ** 2) or 1.0
            nworld = [nworld[0] / length, nworld[1] / length, nworld[2] / length]
            nv.append((di, world[0], world[1], world[2], nworld[0], nworld[1], nworld[2], v[7], v[8]))
        out.append((mat, nv))
    return out


def fit_inc_can_to_smoke(smoke: Smd, inc: Smd) -> list:
    """Cylinder01 can → smoke Bone02, Z-flipped so it sits in the CS 1.6 grip."""
    sm_m, sm_im, sm_w = build_fixup(smoke)
    inc_m, inc_im, inc_w = build_fixup(inc)
    b02 = smoke.name_to_idx["Bone02"]
    cyl = inc.name_to_idx["Cylinder01"]

    smoke_local = []
    for _mat, verts in smoke.tris:
        for v in verts:
            smoke_local.append(to_local([v[1], v[2], v[3]], sm_w[b02], sm_im[b02]))
    inc_local = []
    for _mat, verts in inc.tris:
        for v in verts:
            inc_local.append(to_local([v[1], v[2], v[3]], inc_w[cyl], inc_im[cyl]))

    def centroid(pts):
        n = len(pts)
        return [sum(p[i] for p in pts) / n for i in range(3)]

    def aabb_size(pts):
        lo = [min(p[i] for p in pts) for i in range(3)]
        hi = [max(p[i] for p in pts) for i in range(3)]
        return [hi[i] - lo[i] for i in range(3)]

    cs, ci = centroid(smoke_local), centroid(inc_local)
    ss, si = aabb_size(smoke_local), aabb_size(inc_local)
    scales = [(ss[i] / si[i]) if si[i] > 0.01 else 1.0 for i in range(3)]
    scale = sorted(scales)[1]  # median so one long axis does not dominate

    out = []
    for _mat, verts in inc.tris:
        nv = []
        for v in verts:
            loc = to_local([v[1], v[2], v[3]], inc_w[cyl], inc_im[cyl])
            # Inc can extends +Z, smoke can extends -Z in Bone02.
            mapped = [
                (loc[0] - ci[0]) * scale + cs[0],
                (loc[1] - ci[1]) * scale + cs[1],
                -(loc[2] - ci[2]) * scale + cs[2],
            ]
            world = to_world(mapped, sm_w[b02], sm_m[b02])
            nloc = to_local([v[4], v[5], v[6]], [0.0, 0.0, 0.0], inc_im[cyl])
            nmap = [nloc[0], nloc[1], -nloc[2]]
            nworld = vxf(nmap, sm_m[b02])
            length = math.sqrt(nworld[0] ** 2 + nworld[1] ** 2 + nworld[2] ** 2) or 1.0
            nworld = [nworld[0] / length, nworld[1] / length, nworld[2] / length]
            nv.append((b02, world[0], world[1], world[2], nworld[0], nworld[1], nworld[2], v[7], v[8]))
        out.append(("incendiary_grenade.bmp", nv))
    return out


def write_molotov_qc(path: Path) -> None:
    path.write_text(
        """\
$modelname v_molotov.mdl
$cd .
$cdtexture ./maps_8bit

$body studio "f_molotov_bottle"
$body studio "rhand"
$body studio "lhand"

$attachment 0 "ragslave2" -0.4 0 0.8
$attachment 1 "Bone_Righthand" 0 2.5 1.68

$sequence idle "./anims/idle" fps 30
$sequence pullpin {
    "./anims/pullpin"
    fps 50
    { event 5011 23 "10" }
    { event 5001 24 "10" }
    { event 5001 25 "20" }
    { event 5001 26 "30" }
    { event 5001 27 "20" }
    { event 5001 28 "20" }
    { event 5001 29 "10" }
    { event 5004 23 "weapons/molotov_light.wav" }
}
$sequence throw "./anims/throw" fps 30
$sequence deploy "./anims/deploy" fps 30
""",
        encoding="utf-8",
    )


def write_inc_qc(path: Path, modelname: str, bodies: str, sequences: str) -> None:
    path.write_text(
        f"$modelname {modelname}\n$cd .\n$cdtexture ./maps_8bit\n\n{bodies}\n\n{sequences}\n",
        encoding="utf-8",
    )


def build_molotov(decomp: Path, studiomdl: Path, work: Path, he_mdl: Path, molotov_mdl: Path, out_mdl: Path) -> None:
    he_dir = decompile(decomp, he_mdl, work / "src_he")
    mo_dir = decompile(decomp, molotov_mdl, work / "src_mo")
    dst = work / "v_molotov"
    dst.mkdir(parents=True, exist_ok=True)

    bottle = parse_smd(mo_dir / "f_f_flashbang_template.smd")
    rhand = parse_smd(he_dir / "rhand.smd")
    lhand = parse_smd(he_dir / "lhand.smd")
    dest_fixup = build_fixup(bottle)

    bottle_tris = filter_bottle_tris(bottle)
    if len(bottle_tris) < 50:
        raise SystemExit(f"molotov bottle mesh too small after hand strip ({len(bottle_tris)} tris)")

    write_smd(dst / "f_molotov_bottle.smd", bottle.nodes, bottle.bones, bottle_tris)
    write_smd(dst / "rhand.smd", bottle.nodes, bottle.bones, retarget_hand(rhand, bottle.nodes, bottle.bones, dest_fixup))
    write_smd(dst / "lhand.smd", bottle.nodes, bottle.bones, retarget_hand(lhand, bottle.nodes, bottle.bones, dest_fixup))
    shutil.copytree(mo_dir / "anims", dst / "anims", dirs_exist_ok=True)
    copy_textures(he_dir / "maps_8bit", dst / "maps_8bit", ["view_glove.bmp", "view_skin.bmp", "view_finger.bmp"])
    copy_textures(mo_dir / "maps_8bit", dst / "maps_8bit", ["zippo.bmp", "bottle_mp2.bmp", "rag.bmp", "girardinkriek.bmp"])
    write_molotov_qc(dst / "v_molotov.qc")
    built = compile_qc(studiomdl, dst / "v_molotov.qc", "v_molotov.mdl")
    out_mdl.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, out_mdl)
    print(f"v_molotov.mdl: CS 1.6 hands retargeted onto 2008 Zippo rig ({len(bottle_tris)} bottle tris) -> {out_mdl}")


def build_inc_view(
    decomp: Path, studiomdl: Path, work: Path, smoke_mdl: Path, p_inc_mdl: Path, inc_tex: Path, out_mdl: Path
) -> None:
    sm_dir = decompile(decomp, smoke_mdl, work / "src_smoke_v")
    p_dir = decompile(decomp, p_inc_mdl, work / "src_p_inc_v")
    dst = work / "v_incgrenade"
    shutil.copytree(sm_dir, dst, dirs_exist_ok=True)

    smoke = parse_smd(dst / "f_smokegrenade_template.smd")
    inc = parse_smd(p_dir / "reference_flashbang.smd")
    write_smd(dst / "f_incgrenade_can.smd", smoke.nodes, smoke.bones, fit_inc_can_to_smoke(smoke, inc))
    shutil.copy2(inc_tex, dst / "maps_8bit" / "incendiary_grenade.bmp")

    qc = dst / "v_incgrenade.qc"
    write_inc_qc(
        qc,
        "v_incgrenade.mdl",
        '$body studio "rhand"\n$body studio "lhand"\n$body studio "f_incgrenade_can"',
        """\
$sequence idle "./anims/idle" fps 30
$sequence pullpin {
    "./anims/pullpin"
    fps 41
    { event 5004 27 "weapons/pinpull.wav" }
}
$sequence throw "./anims/throw" fps 30
$sequence deploy "./anims/deploy" fps 30
""",
    )
    built = compile_qc(studiomdl, qc, "v_incgrenade.mdl")
    shutil.copy2(built, out_mdl)
    print(f"v_incgrenade.mdl: CS 1.6 smoke hands + p_inc can on Bone02 -> {out_mdl}")


def build_inc_world(decomp: Path, studiomdl: Path, work: Path, world_mdl: Path, inc_tex: Path, out_mdl: Path) -> None:
    src = decompile(decomp, world_mdl, work / "src_w_inc")
    dst = work / "w_incgrenade"
    shutil.copytree(src, dst, dirs_exist_ok=True)
    painted = replace_first_texture(
        dst / "maps_8bit",
        inc_tex,
        ["f_body.bmp", "smoke_grenade3.bmp", "smoke_body.bmp"],
        (256, 256),
    )
    qc = dst / "w_incgrenade.qc"
    write_inc_qc(
        qc,
        "w_incgrenade.mdl",
        '$body studio "world_flashbang"',
        """\
$sequence idle "./anims/idle" fps 30
$sequence roll1 "./anims/roll1" fps 15
$sequence roll2 "./anims/roll2" fps 15
$sequence roll3 "./anims/roll3" fps 15
$sequence toss1 "./anims/toss1" fps 15
$sequence toss2 "./anims/toss2" fps 15
$sequence toss3 "./anims/toss3" fps 15
""",
    )
    built = compile_qc(studiomdl, qc, "w_incgrenade.mdl")
    shutil.copy2(built, out_mdl)
    print(f"w_incgrenade.mdl: toss sequences + {painted} -> {out_mdl}")


def extract_molotov_from_zip(zip_path: Path, dest: Path) -> Path | None:
    if not zip_path.is_file():
        return None
    import zipfile

    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path) as zf:
        names = [n for n in zf.namelist() if n.lower().endswith("v_molotov.mdl")]
        if not names:
            return None
        member = names[0]
        zf.extract(member, dest)
        return dest / member


def find_inc_texture(cstrike: Path, work: Path, decomp: Path) -> tuple[Path, Path]:
    p_inc = cstrike / "models" / "p_incgrenade.mdl"
    if not p_inc.is_file():
        raise SystemExit("p_incgrenade.mdl fehlt — zuerst ./scripts/import-molotov-assets.sh")
    pdir = decompile(decomp, p_inc, work / "src_p_inc")
    maps = pdir / "maps_8bit"
    matches = list(maps.glob("*.bmp")) + list(maps.glob("*.BMP"))
    tex = next((m for m in matches if "incendiary" in m.name.lower() or "incgrenade" in m.name.lower()), None)
    if tex is None and matches:
        tex = matches[0]
    if tex is None:
        raise SystemExit("incendiary texture missing on p_incgrenade")
    return p_inc, tex


def main() -> int:
    cstrike = gamedata_cstrike()
    models = cstrike / "models"
    he = models / "v_hegrenade.mdl"
    smoke_v = models / "v_smokegrenade.mdl"
    if not he.is_file() or not smoke_v.is_file():
        print("gamedata/cstrike/models/v_hegrenade.mdl oder v_smokegrenade.mdl fehlt.", file=sys.stderr)
        return 1

    build_molotov_view = os.environ.get("CSRETRO_SKIP_MOLOTOV_RETARGET") != "1"
    if not build_molotov_view:
        print("Fire-viewmodels unverändert: intakte importierte MDLs bleiben erhalten.")
        return 0

    decomp = which_tool("CSRETRO_DECOMPMDL", [Path("/tmp/halflife-tools/bin/decompmdl")])
    studiomdl = which_tool("CSRETRO_STUDIOMDL", [Path("/tmp/primext/primext/devkit/pxstudiomdl")])
    if not decomp or not studiomdl:
        print("decompmdl/pxstudiomdl nicht gefunden. CSRETRO_DECOMPMDL und CSRETRO_STUDIOMDL setzen.", file=sys.stderr)
        return 2

    zip_path = Path(os.environ.get("CSRETRO_MOLOTOV_VIEW", "/home/benny/Downloads/molotov_cocktail-3.30_cstrike.zip"))
    current_molotov = models / "v_molotov.mdl"

    with tempfile.TemporaryDirectory(prefix="csretro-fire-vm-") as tmp:
        work = Path(tmp)
        molotov_src = extract_molotov_from_zip(zip_path, work / "zip")
        if molotov_src is None:
            if not current_molotov.is_file():
                print("Kein 2008-v_molotov (Zip fehlt und gamedata auch).", file=sys.stderr)
                return 1
            print("Warnung: CSRETRO_MOLOTOV_VIEW fehlt, nutze vorhandenes v_molotov.mdl als Flaschen-Quelle.")
            molotov_src = current_molotov

        if build_molotov_view:
            build_molotov(decomp, studiomdl, work, he, molotov_src, models / "v_molotov.mdl")
        else:
            print("v_molotov.mdl: intaktes 2008-Zippo-Modell beibehalten (Retarget deaktiviert).")

    print("Fire-viewmodels nach gamedata geschrieben (nicht committen).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
