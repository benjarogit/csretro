#!/usr/bin/env bash
# Copy Molotov/Incendiary models, sprites and sounds into local gamedata/.
# Does not vendor AMXX, delta.lst or anything into git.
#
# After the RAR/Zippo copies, viewmodels stay as imported. Do not rebuild them
# onto HE hands unless CSRETRO_HE_HANDS=1 (owner: keep Zippo / Fire-Pack).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAMEDATA="${CSRETRO_GAMEDATA:-${ROOT}/gamedata}"
RAR="${1:-/home/benny/Downloads/Molotov Incendiary Grenade.rar}"
CSTRIKE="${GAMEDATA}/cstrike"

if [[ ! -f "${RAR}" ]]; then
	echo "RAR fehlt: ${RAR}" >&2
	exit 1
fi
if [[ ! -d "${CSTRIKE}" ]]; then
	echo "gamedata/cstrike fehlt. Zuerst python3 ./scripts/bootstrap-gamedata.py" >&2
	exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

unrar x -o+ -idq "${RAR}" "${TMP}/"
SRC="${TMP}/Molotov Incendiary Grenade/cstrike"

mkdir -p \
	"${CSTRIKE}/models" \
	"${CSTRIKE}/sound/weapons/grenade" \
	"${CSTRIKE}/sprites/grenade" \
	"${CSTRIKE}/events"

copy_renamed() {
	local from="$1"
	local to="$2"
	if [[ ! -f "${from}" ]]; then
		echo "fehlt in RAR: ${from}" >&2
		exit 1
	fi
	cp -f "${from}" "${to}"
}

copy_renamed "${SRC}/models/grenade/v_molotovgrenade.mdl" "${CSTRIKE}/models/v_molotov.mdl"
copy_renamed "${SRC}/models/grenade/p_molotovgrenade.mdl" "${CSTRIKE}/models/p_molotov.mdl"
copy_renamed "${SRC}/models/grenade/w_molotovgrenade.mdl" "${CSTRIKE}/models/w_molotov.mdl"
copy_renamed "${SRC}/models/grenade/v_incendiarygrenade.mdl" "${CSTRIKE}/models/v_incgrenade.mdl"
copy_renamed "${SRC}/models/grenade/p_incendiarygrenade.mdl" "${CSTRIKE}/models/p_incgrenade.mdl"
copy_renamed "${SRC}/models/grenade/w_incendiarygrenade.mdl" "${CSTRIKE}/models/w_incgrenade.mdl"

# The supplied Incendiary viewmodel is already a complete GoldSrc model with
# matching hands, can, lever and four grenade sequences. Two embedded cosmetic
# sound events reference files that are not part of the pack; disable only
# those events while keeping the real pin-pull event intact.
CSRETRO_INC_VIEW="${CSTRIKE}/models/v_incgrenade.mdl" python3 - <<'PY'
from pathlib import Path
import os
import struct

path = Path(os.environ["CSRETRO_INC_VIEW"])
data = bytearray(path.read_bytes())
numseq, seqindex = struct.unpack_from("<ii", data, 164)
disabled = 0
for sequence in range(numseq):
    desc = seqindex + sequence * 176
    numevents, eventindex = struct.unpack_from("<ii", data, desc + 48)
    for event_number in range(numevents):
        event = eventindex + event_number * 76
        option = bytes(data[event + 12:event + 76]).split(b"\0", 1)[0]
        if option in (b"weapons/he_draw.wav", b"weapons/grenade_throw.wav"):
            struct.pack_into("<i", data, event + 4, 0)
            disabled += 1
path.write_bytes(data)
print(f"v_incgrenade.mdl: {disabled} fehlende Soundevents deaktiviert; Geometrie/Animationen unverändert.")
PY

# 2008 AlliedMods GoldSrc bottle (idle/pullpin/throw/deploy, Zippo-Events).
# Replaces the Source-port v_ (idle/draw hide the hands) and material_348
# (CSO-13-Bone-Remesh, in der First-Person unsichtbar).
VIEW_ZIP="${CSRETRO_MOLOTOV_VIEW:-/home/benny/Downloads/molotov_cocktail-3.30_cstrike.zip}"
if [[ -f "${VIEW_ZIP}" ]]; then
	VIEW_TMP="$(mktemp -d)"
	unzip -qo "${VIEW_ZIP}" -d "${VIEW_TMP}"
	if [[ -f "${VIEW_TMP}/models/molotov/v_molotov.mdl" ]]; then
		cp -f "${VIEW_TMP}/models/molotov/v_molotov.mdl" "${CSTRIKE}/models/v_molotov.mdl"
		cp -f "${VIEW_TMP}/models/molotov/p_molotov.mdl" "${CSTRIKE}/models/p_molotov.mdl"
		cp -f "${VIEW_TMP}/models/molotov/w_broke_molotov.mdl" "${CSTRIKE}/models/w_broke_molotov.mdl"
		echo "v_/p_molotov + w_broke aus ${VIEW_ZIP} (GoldSrc 2008)."
	fi
	rm -rf "${VIEW_TMP}"
fi

# Owner-selected green bottle / lighter skin. Binary comparison confirms the
# same mesh, skeleton and four sequences as the 2008 viewmodel above.
SKIN_RAR="${CSRETRO_MOLOTOV_SKIN:-/home/benny/Downloads/grenades_09_2.rar}"
if [[ -f "${SKIN_RAR}" ]]; then
	SKIN_TMP="$(mktemp -d)"
	unrar x -inul "${SKIN_RAR}" 'cstrike/models/v_hegrenade.mdl' "${SKIN_TMP}/"
	if [[ -f "${SKIN_TMP}/cstrike/models/v_hegrenade.mdl" ]]; then
		cp -f "${SKIN_TMP}/cstrike/models/v_hegrenade.mdl" "${CSTRIKE}/models/v_molotov.mdl"
		echo "Molotov-Viewmodel-Skin aus ${SKIN_RAR}; Animationsrig unverändert."
	fi
fi

if [[ -f "${CSTRIKE}/sound/weapons/pinpull.wav" && ! -f "${CSTRIKE}/sound/weapons/molotov_light.wav" ]]; then
	cp -f "${CSTRIKE}/sound/weapons/pinpull.wav" "${CSTRIKE}/sound/weapons/molotov_light.wav"
fi

cp -f "${SRC}/sound/weapons/grenade/"*.wav "${CSTRIKE}/sound/weapons/grenade/"
cp -f "${SRC}/sprites/grenade/"*.spr "${CSTRIKE}/sprites/grenade/"

# HUD txt and the event live in the repo; bootstrap/ui-overrides also copy them.
if [[ -f "${ROOT}/data/ui-overrides/cstrike/sprites/weapon_molotov.txt" ]]; then
	cp -f "${ROOT}/data/ui-overrides/cstrike/sprites/weapon_molotov.txt" "${CSTRIKE}/sprites/weapon_molotov.txt"
	cp -f "${ROOT}/data/ui-overrides/cstrike/sprites/weapon_incgrenade.txt" "${CSTRIKE}/sprites/weapon_incgrenade.txt"
fi
if [[ -f "${ROOT}/data/ui-overrides/cstrike/events/createinferno.sc" ]]; then
	cp -f "${ROOT}/data/ui-overrides/cstrike/events/createinferno.sc" "${CSTRIKE}/events/createinferno.sc"
fi

HUD="${CSTRIKE}/sprites/hud.txt"
if [[ -f "${HUD}" ]]; then
	if ! grep -q '^d_molotov' "${HUD}"; then
		printf '%s\n' \
			'd_molotov			640 grenade/hud_molotov	0	0	32	16' \
			'd_incgrenade			640 grenade/hud_molotov	0	47	32	16' \
			>> "${HUD}"
	fi
	export HUD
	python3 - <<'PY'
from pathlib import Path
import os
path = Path(os.environ["HUD"])
lines = path.read_text(encoding="utf-8").splitlines()
records = [line for line in lines[1:] if line.strip() and not line.strip().startswith("//") and len(line.split()) == 7]
if not lines:
    raise SystemExit("leeres hud.txt")
lines[0] = str(len(records))
path.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"hud.txt: {len(records)} Sprite-Datensätze")
PY
fi

# Stock CS sends m_iId in 5 bits (0–31). IDs 32/33 must use 6 bits.
# Not an AMXX delta.lst — same file, wider weapon id.
DELTA="${CSTRIKE}/delta.lst"
if [[ -f "${DELTA}" ]]; then
	CSRETRO_DELTA_LST="${DELTA}" python3 - <<'PY'
from pathlib import Path
import os, re
path = Path(os.environ["CSRETRO_DELTA_LST"])
text = path.read_text(encoding="utf-8")
patched, n = re.subn(
    r"(DEFINE_DELTA\(\s*m_iId,\s*DT_INTEGER,\s*)5(,\s*1\.0\s*\))",
    r"\g<1>6\2",
    text,
)
if n:
    path.write_text(patched, encoding="utf-8")
    print(f"delta.lst: m_iId auf 6 Bit ({n} Felder).")
PY
fi

# Optional languagelawyer HE-hands rebuild. Owner: Zippo / Fire-Pack stay.
# Only with CSRETRO_HE_HANDS=1.
rebuild_he_hands() {
	local mdldec="${CSRETRO_MDLDEC:-${ROOT}/build/tools/mdldec}"
	local studiomdl="${CSRETRO_STUDIOMDL:-${ROOT}/build/tools/studiomdl/build/bin-x86_64/studiomdl}"
	local activities="${CSRETRO_ACTIVITIES:-${ROOT}/build/tools/activities.txt}"
	if [[ "${CSRETRO_HE_HANDS:-0}" == "0" ]]; then
		echo "Viewmodels unverändert (Zippo/Fire-Pack). CSRETRO_HE_HANDS=1 wäre der languagelawyer-Rebuild."
		return 0
	fi
	if [[ ! -x "${mdldec}" || ! -x "${studiomdl}" || ! -f "${activities}" ]]; then
		echo "HE-Hände-Rebuild übersprungen: mdldec/studiomdl/activities.txt fehlen unter build/tools/." >&2
		return 0
	fi
	python3 "${ROOT}/scripts/build_molotov_models.py" \
		--game-dir "${CSTRIKE}" \
		--mdldec "${mdldec}" \
		--studiomdl "${studiomdl}" \
		--activities "${activities}" \
		--output-dir "${CSTRIKE}/models" \
		--force
	python3 "${ROOT}/scripts/build_incgrenade_viewmodel.py" \
		--game-dir "${CSTRIKE}" \
		--mdldec "${mdldec}" \
		--studiomdl "${studiomdl}" \
		--activities "${activities}" \
		--output-dir "${CSTRIKE}/models" \
		--force
}

rebuild_he_hands

echo "Molotov/Incendiary-Assets nach ${CSTRIKE} kopiert (nicht committen)."
echo "Kein AMXX. delta.lst: nur m_iId 5→6 Bit für IDs 32/33."
