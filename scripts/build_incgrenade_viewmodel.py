#!/usr/bin/env python3
"""Build v_incgrenade.mdl with stock CS 1.6 HE hands and pin/throw/deploy.

Recipe from languagelawyer/cs16-client (molotov branch): keep the HE
skeleton, animations and gloves; only replace the grenade body. The can
mesh comes from local p_incgrenade (Fire-Pack), fitted into Bone02.

Requires the same mdldec/studiomdl pair as scripts/build_molotov_models.py.
"""

from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

Q_PI = math.pi
HE_BONE = 36  # Bone02 on decompiled v_hegrenade
CYL_BONE = 11  # Cylinder01 on p_incgrenade
HE_MID = (2.4198225, -16.046101, 2.9988515)
HE_SIZE_Z = 4.403985


def run_tool(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    result = subprocess.run(
        command, cwd=cwd, env=env or os.environ.copy(),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace",
    )
    if result.returncode:
        raise SystemExit(f"{Path(command[0]).name} failed ({result.returncode}):\n{result.stdout}")


def decompile(mdldec: Path, activities: Path, mdl: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["MDLDEC_ACT_PATH"] = str(activities.parent)
    run_tool([str(mdldec), "-m", str(mdl), str(dest)], cwd=dest, env=env)


def parse_smd(path: Path):
    lines = path.read_text(encoding="latin-1").splitlines()
    nodes = []
    bones = {}
    tris = []
    header = []
    mode = None
    i = 0
    while i < len(lines):
        raw = lines[i]
        s = raw.strip()
        if mode is None and s not in ("nodes", "skeleton", "triangles"):
            header.append(raw)
            i += 1
            continue
        if s == "nodes":
            mode = "n"
            i += 1
            continue
        if s == "skeleton":
            mode = "s"
            i += 1
            continue
        if s == "triangles":
            mode = "t"
            i += 1
            continue
        if s == "end":
            mode = None
            i += 1
            continue
        if mode == "n" and s:
            q = s.split('"')
            nodes.append((int(q[0].split()[0]), q[1], int(q[2].split()[0])))
        elif mode == "s" and s and not s.startswith("time"):
            p = s.split()
            bones[int(p[0])] = (tuple(map(float, p[1:4])), tuple(map(float, p[4:7])))
        elif mode == "t":
            verts = []
            for k in range(1, 4):
                p = lines[i + k].split()
                verts.append(tuple([int(p[0])] + [float(x) for x in p[1:9]]))
            tris.append((s, verts))
            i += 3
        i += 1
    return header, nodes, bones, tris


def angle_matrix(angles):
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


def angle_imatrix(angles):
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


def concat(a, b):
    out = [[0.0] * 4 for _ in range(3)]
    for i in range(3):
        for j in range(3):
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]
        out[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3]
    return out


def vxf(v, m):
    return [
        v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2] + m[0][3],
        v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2] + m[1][3],
        v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2] + m[2][3],
    ]


def bone_world(nodes, bones):
    m_out, im, world = {}, {}, {}
    for idx, _name, parent in nodes:
        pos, rot = bones[idx]
        ang = [rot[0] * 180 / Q_PI, rot[1] * 180 / Q_PI, rot[2] * 180 / Q_PI]
        if parent == -1:
            m_out[idx] = angle_matrix(ang)
            im[idx] = angle_imatrix(ang)
            world[idx] = list(pos)
        else:
            m_out[idx] = concat(m_out[parent], angle_matrix(ang))
            im[idx] = concat(angle_imatrix(ang), im[parent])
            p = vxf(list(pos), m_out[parent])
            world[idx] = [p[0] + world[parent][0], p[1] + world[parent][1], p[2] + world[parent][2]]
    return m_out, im, world


def to_local(p, w, imat):
    return vxf([p[0] - w[0], p[1] - w[1], p[2] - w[2]], imat)


def fit_can(he_header: list[str], inc_path: Path) -> str:
    _h, nodes, bones, tris = parse_smd(inc_path)
    _m, im, world = bone_world(nodes, bones)
    local_pts = []
    for _mat, verts in tris:
        for v in verts:
            local_pts.append(to_local([v[1], v[2], v[3]], world[CYL_BONE], im[CYL_BONE]))
    lo = [min(p[i] for p in local_pts) for i in range(3)]
    hi = [max(p[i] for p in local_pts) for i in range(3)]
    mid = [(lo[i] + hi[i]) / 2 for i in range(3)]
    scale = HE_SIZE_Z / (hi[2] - lo[2])
    lines = list(he_header)
    if lines[-1].strip() != "triangles":
        # Keep HE nodes/skeleton; rewrite only the triangle block.
        pass
    # he_header already contains version/nodes/skeleton/triangles from the HE template.
    out = []
    for mat, verts in tris:
        out.append("incendiary_grenade.bmp")
        for v in verts:
            loc = to_local([v[1], v[2], v[3]], world[CYL_BONE], im[CYL_BONE])
            pos = [(loc[i] - mid[i]) * scale + HE_MID[i] for i in range(3)]
            nloc = to_local([v[4], v[5], v[6]], [0.0, 0.0, 0.0], im[CYL_BONE])
            length = math.sqrt(sum(c * c for c in nloc)) or 1.0
            nrm = [c / length for c in nloc]
            out.append(
                f" {HE_BONE} {pos[0]:.6f} {pos[1]:.6f} {pos[2]:.6f} "
                f"{nrm[0]:.6f} {nrm[1]:.6f} {nrm[2]:.6f} {v[7]:.6f} {v[8]:.6f}"
            )
    out.append("end")
    return "\n".join(out) + "\n"


def smd_prefix_until_triangles(path: Path) -> str:
    lines = path.read_text(encoding="latin-1").splitlines()
    cut = lines.index("triangles")
    return "\n".join(lines[: cut + 1]) + "\n"


def replace_qc(source: Path, model_name: str, body: str) -> str:
    text = source.read_text()
    text, names = re.subn(r'\$modelname\s+"[^"]+"', f'$modelname "{model_name}"', text)
    text, bodies = re.subn(
        r'\$body\s+"' + body + r'"\s+"[^"]+"',
        f'$body "{body}" "incgrenade_can"',
        text,
    )
    if names != 1 or bodies != 1:
        raise SystemExit(f"unexpected QC layout in {source}")
    return text


def build(game_dir: Path, output: Path, mdldec: Path, studiomdl: Path, activities: Path, force: bool) -> None:
    he = game_dir / "models" / "v_hegrenade.mdl"
    p_inc = game_dir / "models" / "p_incgrenade.mdl"
    tex = None
    if not he.is_file() or not p_inc.is_file():
        raise SystemExit("v_hegrenade.mdl und p_incgrenade.mdl müssen in --game-dir/models liegen")
    target = output / "v_incgrenade.mdl"
    if target.exists() and not force:
        raise SystemExit(f"{target} existiert; --force zum Überschreiben")
    with tempfile.TemporaryDirectory(prefix="incgrenade-vm-") as workspace:
        work = Path(workspace)
        decompile(mdldec, activities, he, work / "he")
        decompile(mdldec, activities, p_inc, work / "pinc")
        he_dir = work / "he"
        dst = work / "compiled"
        dst.mkdir()
        for smd in he_dir.glob("*.smd"):
            if smd.stem != "f_hegrenade_template":
                (dst / smd.name).write_bytes(smd.read_text(encoding="latin-1").encode())
        for tex_name in ("view_finger.bmp", "view_glove.bmp", "view_skin.BMP"):
            shutil.copyfile(he_dir / tex_name, dst / tex_name)
        pinc_maps = list((work / "pinc").glob("incendiary_grenade.bmp")) + list((work / "pinc").glob("*.bmp"))
        if not pinc_maps:
            raise SystemExit("incendiary_grenade.bmp fehlt auf p_incgrenade")
        inc_tex = next((p for p in pinc_maps if "incendiary" in p.name.lower()), pinc_maps[0])
        shutil.copyfile(inc_tex, dst / "incendiary_grenade.bmp")
        prefix = smd_prefix_until_triangles(he_dir / "f_hegrenade_template.smd")
        can_body = fit_can(prefix.splitlines(), work / "pinc" / "reference_flashbang.smd")
        (dst / "incgrenade_can.smd").write_bytes((prefix + can_body).encode())
        qc = dst / "v_incgrenade.qc"
        qc.write_bytes(replace_qc(he_dir / "v_hegrenade.qc", "v_incgrenade.mdl", "weapon").encode())
        print("Compiling v_incgrenade.mdl", flush=True)
        run_tool([str(studiomdl), qc.name], cwd=dst)
        built = dst / "v_incgrenade.mdl"
        if not built.is_file():
            raise SystemExit("studiomdl wrote no v_incgrenade.mdl")
        output.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(built, target)
        print(f"Built {target}")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--game-dir", type=Path, required=True)
    p.add_argument("--output-dir", type=Path, required=True)
    p.add_argument("--mdldec", type=Path, required=True)
    p.add_argument("--studiomdl", type=Path, required=True)
    p.add_argument("--activities", type=Path, required=True)
    p.add_argument("--force", action="store_true")
    args = p.parse_args()
    build(
        args.game_dir.expanduser().resolve(),
        args.output_dir.expanduser().resolve(),
        args.mdldec.expanduser().resolve(),
        args.studiomdl.expanduser().resolve(),
        args.activities.expanduser().resolve(),
        args.force,
    )


if __name__ == "__main__":
    main()
