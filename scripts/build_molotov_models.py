#!/usr/bin/env python3
"""Build Molotov models from a local, stock Counter-Strike installation.

Copied locally from languagelawyer/cs16-client (molotov branch) on 2026-09-11.
Not a submodule. Selective port: HE hands/animations + lv_bottle mesh.

Requires Python 3.9+, FWGS mdldec and a GoldSrc studiomdl. No game content is
embedded or downloaded. The recipe retains HE hands/animations, replaces the
grenade mesh with lv_bottle, and adds a procedural cloth wick.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile


# The placement and bone indices below target these stock models, not arbitrary
# replacement skins or the optional HD pack. Hashes identify inputs, not assets.
SOURCE_HASHES = {
    "lv_bottle": "10c5234bfd06812235ad277e149f6a0cb3b54cefc20fac614795a21e941ecb00",
    "v_hegrenade": "86f6a5ea2f4fa80b4ce86abb62007b69ea3c500ca0531498630bad1230b0ff41",
    "p_hegrenade": "494b329493f545298cdaa434edf70f32bafad964daf4d7a5c6383f8ef3f5d9cb",
    "w_hegrenade": "59bca29c4207ebfaab9f65e4b61dc26bfffa7cc2eb86b761bb11ca7b025e4458",
}
MODEL_NAMES = tuple(f"{kind}_molotov.mdl" for kind in ("v", "p", "w"))
# kind, reference mesh, attachment bone, scale, translation, orientation
DEFINITIONS = (
    ("v", "f_hegrenade_template", 36, 0.46, (2.35, -16.0, -1.0), "upright"),
    ("p", "reference_flashbang", 11, 0.48, (-16.0, 0.0, 42.5), "upright"),
    ("w", "world_flashbang", 0, 0.66, (-6.55, 0.0, 0.0), "length_x"),
)


def check_sources(game_dir: Path) -> None:
    for name, expected in SOURCE_HASHES.items():
        path = game_dir / "models" / f"{name}.mdl"
        if not path.is_file():
            raise ValueError(f"missing {path}; --game-dir must point to cstrike")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != expected:
            raise ValueError(
                f"unsupported source model: {path}\n"
                f"  expected SHA-256: {expected}\n  actual SHA-256:   {digest}\n"
                "Use the stock CS 1.6 models, without replacement skins/HD models."
            )


def resolve_tool(value: str) -> Path:
    found = shutil.which(str(Path(value).expanduser()))
    if not found:
        raise ValueError(f"executable not found: {value}")
    return Path(found).resolve()


def run_tool(command: list[str], cwd: Path, env: dict[str, str]) -> None:
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    if result.returncode:
        raise ValueError(f"{Path(command[0]).name} failed ({result.returncode}):\n{result.stdout}")


def decompile(game_dir: Path, destination: Path, mdldec: Path,
              activities: Path | None) -> None:
    env = os.environ.copy()
    if activities is None:
        candidates = [mdldec.parent, mdldec.parent / "res"]
        if env.get("MDLDEC_ACT_PATH"):
            candidates.insert(0, Path(env["MDLDEC_ACT_PATH"]).expanduser())
        activities = next((p / "activities.txt" for p in candidates
                           if (p / "activities.txt").is_file()), None)
    if activities is None or not activities.is_file():
        raise ValueError("mdldec needs activities.txt; use --activities /path/to/activities.txt")
    # mdldec looks in cwd first and MDLDEC_ACT_PATH second. Keep cwd private.
    env["MDLDEC_ACT_PATH"] = str(activities.resolve().parent)
    if activities.name != "activities.txt":
        raise ValueError("--activities must name mdldec's activities.txt")
    for name in SOURCE_HASHES:
        output = destination / name
        output.mkdir(parents=True)
        print(f"Decompiling {name}.mdl", flush=True)
        run_tool([str(mdldec), "-m", str(game_dir / "models" / f"{name}.mdl"),
                  str(output)], cwd=destination, env=env)
        if not (output / f"{name}.qc").is_file():
            raise ValueError(f"mdldec did not produce {name}.qc")


def smd_header(path: Path) -> list[str]:
    lines = path.read_text().splitlines()
    return lines[:lines.index("triangles") + 1]


def smd_triangles(path: Path) -> list[tuple[str, list[list[str]]]]:
    lines = path.read_text().splitlines()
    cursor = lines.index("triangles") + 1
    triangles = []
    while lines[cursor] != "end":
        triangles.append((lines[cursor].strip(),
                          [lines[cursor + offset].split() for offset in (1, 2, 3)]))
        cursor += 4
    return triangles


def transform_vector(vector: tuple[float, float, float], orientation: str) -> tuple[float, float, float]:
    x, y, z = vector
    if orientation == "upright":
        return x, y, z
    if orientation == "length_x":
        # Projectile angles point local X along the flight velocity.
        return z, x, y
    raise ValueError(f"unknown orientation: {orientation}")


def transform_vertex(fields: list[str], bone: int, scale: float,
                     translation: tuple[float, float, float], orientation: str) -> str:
    position = transform_vector(tuple(map(float, fields[1:4])), orientation)
    normal = transform_vector(tuple(map(float, fields[4:7])), orientation)
    length = math.sqrt(sum(component * component for component in normal)) or 1.0
    position = tuple(position[i] * scale + translation[i] for i in range(3))
    normal = tuple(component / length for component in normal)
    return (f"  {bone} {position[0]:.6f} {position[1]:.6f} {position[2]:.6f} "
            f"{normal[0]:.6f} {normal[1]:.6f} {normal[2]:.6f} {fields[7]} {fields[8]}")


def rag_triangles() -> list[tuple[str, list[list[str]]]]:
    """Two crossed, double-sided cloth strips emerging from the bottle neck."""
    strips = [
        [(-0.9, 0.12, 18.4), (0.9, 0.12, 18.4), (1.5, 0.12, 24.5), (-0.3, 0.12, 25.2)],
        [(0.12, -0.9, 18.4), (0.12, 0.9, 18.4), (0.12, 1.5, 24.5), (0.12, -0.3, 25.2)],
    ]
    uv = [(0.05, 0.95), (0.95, 0.95), (0.95, 0.05), (0.05, 0.05)]
    output = []
    for strip_index, points in enumerate(strips):
        normal = (0.0, 1.0, 0.0) if strip_index == 0 else (1.0, 0.0, 0.0)
        for order, sign in (((0, 1, 2), 1.0), ((0, 2, 3), 1.0), ((2, 1, 0), -1.0), ((3, 2, 0), -1.0)):
            vertices = []
            for i in order:
                vertices.append(["0", *(f"{v:.6f}" for v in points[i]),
                                 *(f"{v * sign:.6f}" for v in normal),
                                 f"{uv[i][0]:.6f}", f"{uv[i][1]:.6f}"])
            output.append(("molotov_rag.bmp", vertices))
    return output


def make_rag_texture(destination: Path) -> None:
    """Write an indexed 64x64 BMP using only the standard library."""
    colors = {}
    pixels = bytearray()
    for y in reversed(range(64)):  # BMP rows are bottom-up, already 4-byte aligned.
        for x in range(64):
            thread = ((x * 5 + y * 3) % 17) - 8
            stripe = -18 if (x + 2 * y) % 23 < 3 else 0
            rgb = (126 + thread + stripe, 103 + thread + stripe, 66 + thread)
            pixels.append(colors.setdefault(rgb, len(colors)))
    palette = b"".join(bytes((b, g, r, 0)) for r, g, b in colors)
    palette = palette.ljust(256 * 4, b"\0")
    offset = 14 + 40 + len(palette)
    header = struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", 40, 64, 64, 1, 8, 0, len(pixels), 0, 0, 256, 0)
    destination.write_bytes(header + info + palette + pixels)


def replace_qc_body(source: Path, model_name: str, body: str) -> str:
    text = source.read_text()
    text, names = re.subn(r'\$modelname\s+"[^"]+"', f'$modelname "{model_name}"', text)
    text, bodies = re.subn(r'\$body\s+"' + body + r'"\s+"[^"]+"',
                          f'$body "{body}" "molotov_bottle"', text)
    if names != 1 or bodies != 1:
        raise ValueError(f"unexpected QC layout in {source.name}; use FWGS mdldec")
    return text


def prepare_models(decompiled: Path, destination: Path) -> list[Path]:
    bottle = smd_triangles(decompiled / "lv_bottle/test.smd") + rag_triangles()
    outputs = []
    for kind, template, bone, scale, translation, orientation in DEFINITIONS:
        source = decompiled / f"{kind}_hegrenade"
        output = destination / kind
        output.mkdir(parents=True)
        for smd in source.glob("*.smd"):
            if smd.stem != template:
                # Normalize line endings for older studiomdl versions.
                (output / smd.name).write_bytes(smd.read_text().encode())
        if kind == "v":
            for texture in ("view_finger.bmp", "view_glove.bmp", "view_skin.BMP"):
                shutil.copyfile(source / texture, output / texture)
        shutil.copyfile(decompiled / "lv_bottle/Untitled-1.bmp", output / "molotov_bottle.bmp")
        make_rag_texture(output / "molotov_rag.bmp")
        lines = smd_header(source / f"{template}.smd")
        for texture, vertices in bottle:
            lines.append("molotov_bottle.bmp" if texture.lower() == "untitled-1.bmp" else texture)
            lines.extend(transform_vertex(v, bone, scale, translation, orientation) for v in vertices)
        lines.append("end")
        (output / "molotov_bottle.smd").write_bytes(("\n".join(lines) + "\n").encode())
        qc = output / f"{kind}_molotov.qc"
        qc.write_bytes(replace_qc_body(source / f"{kind}_hegrenade.qc",
                                      f"{kind}_molotov.mdl", "weapon" if kind == "v" else "studio").encode())
        outputs.append(qc)
    return outputs


def check_model(path: Path) -> None:
    """Reject missing/broken output, wrong animation order or split model files."""
    data = path.read_bytes()
    if (len(data) < 244 or data[:8] != b"IDST\x0a\0\0\0"
            or struct.unpack_from("<i", data, 72)[0] != len(data)):
        raise ValueError(f"invalid GoldSrc MDL output: {path.name}")
    count, offset, groups, _group_offset, textures = struct.unpack_from("<5i", data, 164)
    expected = {"v": ("idle", "pullpin", "throw", "deploy"), "p": ("idle",),
                "w": ("idle", "roll1", "roll2", "roll3", "toss1", "toss2", "toss3")}[path.name[0]]
    if (count != len(expected) or groups != 1 or textures < 1
            or offset < 244 or offset + count * 176 > len(data)):
        raise ValueError(f"unexpected model layout or external textures/sequences: {path.name}")
    labels = tuple(data[offset + i * 176:offset + i * 176 + 32].split(b"\0", 1)[0].decode()
                   for i in range(count))
    if labels != expected:
        raise ValueError(f"animation order changed in {path.name}: {labels}")


def check_destination(output: Path, force: bool) -> None:
    for name in MODEL_NAMES:
        target = output / name
        if target.is_symlink():
            raise ValueError(f"refusing to overwrite symlink: {target}")
        if target.exists() and (not force or not target.is_file()):
            raise ValueError(f"output already exists: {target}; use --force to replace generated models")


def build(game_dir: Path, output: Path, mdldec: Path, studiomdl: Path,
          activities: Path | None, force: bool) -> None:
    check_sources(game_dir)
    check_destination(output, force)
    # No decompiled meshes, textures or animations are left in the source tree.
    with tempfile.TemporaryDirectory(prefix="molotov-models-") as workspace:
        work = Path(workspace)
        decompile(game_dir, work / "decompiled", mdldec, activities)
        models = []
        for qc in prepare_models(work / "decompiled", work / "compiled"):
            print(f"Compiling {qc.stem}.mdl", flush=True)
            run_tool([str(studiomdl), qc.name], cwd=qc.parent, env=os.environ.copy())
            model = qc.with_suffix(".mdl")
            check_model(model)
            models.append(model)
        # Compile and validate the whole set before touching the destination.
        check_destination(output, force)
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".molotov-install-", dir=output) as staging:
            for model in models:
                shutil.copyfile(model, Path(staging) / model.name)
            for model in models:
                os.replace(Path(staging) / model.name, output / model.name)
    print(f"Built {', '.join(MODEL_NAMES)} in {output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, required=True, help="Source cstrike directory with stock models/")
    parser.add_argument("--output-dir", type=Path, default=Path("build/molotov-models"),
                        help="Output model directory (default: build/molotov-models)")
    parser.add_argument("--mdldec", default="mdldec", help="FWGS mdldec executable or PATH name")
    parser.add_argument("--studiomdl", default="studiomdl", help="GoldSrc studiomdl executable or PATH name")
    parser.add_argument("--activities", type=Path, help="mdldec's activities.txt (auto-detected beside mdldec or in res/)")
    parser.add_argument("--force", action="store_true", help="Replace the three generated output models if present")
    args = parser.parse_args()
    try:
        build(args.game_dir.expanduser().resolve(), args.output_dir.expanduser().resolve(),
              resolve_tool(args.mdldec), resolve_tool(args.studiomdl),
              args.activities.expanduser().resolve() if args.activities else None, args.force)
    except (OSError, ValueError, IndexError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    main()
