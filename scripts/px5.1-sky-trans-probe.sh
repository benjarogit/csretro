#!/usr/bin/env bash
# PX5.1: Sky farClip pixelproof + global trans Xash classification.
# Visible frame stays Xash (GL_RenderFrame = 0). No return 1.
# Usage: ./scripts/build-engine.sh && ./scripts/build-client.sh && \
#        ./scripts/px5.1-sky-trans-probe.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-crash.sh"
source "${ROOT}/scripts/gate-isolate.sh"

ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
MAP="de_aztec"

fail() { echo "PX51_SKY_TRANS_PROBE FAIL: $*" >&2; exit 1; }

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" && -f "${GAMEDLL}" && -f "${MENU}" ]] || fail "Binaries fehlen"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/de_dust.bsp" ]] || fail "de_dust.bsp fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/cs_assault.bsp" ]] || fail "cs_assault.bsp fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/de_torn.bsp" ]] || fail "de_torn.bsp fehlt"

export CSRETRO_ENGINE_OUT="$(readlink -f "${ENG}")"
export CSRETRO_CLIENT_SO="$(readlink -f "${CLIENT}")"
export CSRETRO_GAMEDLL_SO="$(readlink -f "${GAMEDLL}")"
export CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
csretro_gate_isolate_begin "px51sky" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${ROOT}/build/px5-vis-shots"
mkdir -p "${SHOTS}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
r_csretro_renderer 1
r_csretro_offscreen_dump 1
r_csretro_probe_seq 14
sv_cheats 1
mp_auto_join_team 1
humans_join_team T
mp_freezetime 0
mp_limitteams 0
mp_autoteambalance 0
bot_enable 1
bot_quota 1
bot_join_after_player 0
sv_lan 1
echo CSRETRO_PX51_SKY_TRANS_PROBE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px51sky.log"

wait_log() {
	local pat="$1" secs="$2" i
	for i in $(seq 1 "${secs}"); do
		if rg -q "${pat}" "${LOG}" 2>/dev/null || rg -q "${pat}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			return 0
		fi
		sleep 1
	done
	return 1
}

csretro_headless_x11_prepare || fail "gamescope/display"
rm -f "${LOG}"
: >"${CSRETRO_GAMESCOPE_LOG}"

cd "${RUN}"
set +e
csretro_headless_x11_wrap ./xash3d \
	-game cstrike \
	-dll "${GAMEDLL}" \
	-clientlib "${CLIENT}" \
	-menulib "${MENU}" \
	-windowed -width 1280 -height 720 \
	-dev 2 -log \
	+maxplayers 8 \
	+sv_lan 1 \
	+map "${MAP}" \
	+exec autoexec.cfg \
	>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
XASH_PID=$!
CSRETRO_GAMESCOPE_PID="${XASH_PID}"
set -e
cd "${ROOT}"

csretro_headless_x11_wait_display || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	fail "X11"
}

wait_log 'vis frame origin=' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein vis frame log"
}
wait_log 'probe_seq sky look' 15 || true
wait_log 'sky pixelproof' 20 || true
wait_log 'world vis total=' 15 || true
wait_log 'Mod_GetCurrentVis active' 10 || true
wait_log 'probe_seq r_novis 1' 25 || true
wait_log 'probe_seq r_novis 0' 15 || true
wait_log 'probe_seq r_lockpvs 1' 15 || true
wait_log 'probe_seq r_lockpvs 0' 15 || true
wait_log 'vis overview=1' 20 || true
if command -v import >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/aztec-sky-trans.png" 2>/dev/null || true
fi
wait_log 'offscreen world map=maps/de_torn' 45 || true
wait_log 'offscreen world map=maps/cs_assault' 30 || true
wait_log 'offscreen world map=maps/de_dust' 30 || true
wait_log 'probe_seq vid_setmode' 25 || true
wait_log 'probe_seq quit' 25 || true
csretro_gate_wait_quit "${XASH_PID}" 20 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px51sky.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true

find "${RUN}" -name 'csretro_sky.ppm' -print0 2>/dev/null | while IFS= read -r -d '' f; do
	cp -f "$f" "${SHOTS}/csretro_sky.ppm" || true
done
find "${RUN}" -name 'csretro_offscreen.ppm' -print0 2>/dev/null | while IFS= read -r -d '' f; do
	cp -f "$f" "${SHOTS}/csretro_offscreen.ppm" || true
done

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi
rg -q 'vis frame origin=' "${ALL}" || fail "current-frame vis fehlt"
rg -q 'pvs_match=1' "${ALL}" || fail "engine/client PVS hash mismatch"
rg -q 'Mod_GetCurrentVis active' "${ALL}" || fail "Mod_GetCurrentVis fehlt"
rg -q 'world vis total=' "${ALL}" || fail "world vis selection fehlt"
rg -q 'drawn=[1-9]' "${ALL}" || fail "world drawn==0"
if ! python3 - "${ALL}" <<'PY'
import re, sys
text = open(sys.argv[1], errors="replace").read()
found = False
partial = False
for m in re.finditer(r"world vis total=(\d+).* drawn=(\d+)", text):
    found = True
    if int(m.group(1)) != int(m.group(2)):
        partial = True
        break
sys.exit(0 if found and partial else 1)
PY
then
	fail "world drawn==all surfaces or missing"
fi
rg -q 'efrag inventory count=0 implemented safety contract|efrag feed count=[1-9]' "${ALL}" || fail "efrag contract fehlt"
rg -q 'trans order count=' "${ALL}" || fail "trans order fehlt"
rg -q 'trans comparator proof' "${ALL}" || fail "trans comparator proof fehlt"
rg -q 'duplicate_scene_draws=0' "${ALL}" || fail "duplicate_scene_draws nicht 0"
rg -q 'alias status runtime NOT REPRODUCIBLE|alias seen index=' "${ALL}" || fail "alias contract fehlt"
rg -q 'backface classifier CONFIRMED code parity' "${ALL}" || fail "backface classifier fehlt"
rg -q 'vis overview=1 prepared=1' "${ALL}" || fail "overview vis seam fehlt"
rg -q 'r_renderscene ownership' "${ALL}" || fail "R_RenderScene ownership log fehlt"
rg -q 'offscreen world map=maps/de_dust' "${ALL}" || fail "Mapchange dust fehlt"
rg -q 'probe_seq vid_setmode 1024 768' "${ALL}" || fail "vid_setmode fehlt"
rg -q 'farclip=' "${ALL}" || fail "farclip log fehlt"

if ! python3 - "${ALL}" <<'PY'
import re, sys
text = open(sys.argv[1], errors="replace").read()
ok = False
for m in re.finditer(
    r"sky pixelproof before=([0-9a-f]+) after=([0-9a-f]+) differ=(\d+) candidates=(\d+) drawn=(\d+) nonempty=(\d+) farclip=([0-9.]+) sides=(\d+)",
    text,
):
    differ = int(m.group(3))
    cand = int(m.group(4))
    drawn = int(m.group(5))
    nonempty = int(m.group(6))
    farclip = float(m.group(7))
    sides = int(m.group(8))
    if differ == 1 and cand > 0 and drawn > 0 and nonempty > 0 and farclip > 0 and sides > 0:
        ok = True
        break
sys.exit(0 if ok else 1)
PY
then
	fail "sky pixelproof unvollständig (farclip/sides/differ/nonempty)"
fi

echo "PX51_SKY_TRANS_PROBE PASS"
echo "PX51_SKY_TRANS_PROBE log=${ALL}"
rg -n 'vis frame|world vis|Mod_GetCurrentVis|efrag|sky pixelproof|trans order|trans cmp|trans comparator|alias |backface |vis overview|r_renderscene ownership|probe_seq vis|probe_seq sky|probe_seq r_|leaf-change|visible frame stays Xash' "${ALL}" | head -n 200 || true
exit 0
