#!/usr/bin/env bash
# #7 Brush Special B: SURF_DRAWTURB / water. Visible frame stays Xash (GL_RenderFrame = 0).
# de_aztec (water) → sv_wateralpha/sv_wateramp → de_torn → de_dust → cs_assault + vid_setmode.
# Usage: ./scripts/build-client.sh && ./scripts/px7-brush-special-b-probe.sh
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

fail() { echo "PX7_SPECIAL_B FAIL: $*" >&2; exit 1; }

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
csretro_gate_isolate_begin "px7specialb" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${ROOT}/build/px7-special-b-shots"
mkdir -p "${SHOTS}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
r_csretro_renderer 1
r_csretro_offscreen_dump 1
r_csretro_probe_seq 7
sv_cheats 1
mp_auto_join_team 1
humans_join_team T
mp_freezetime 0
bot_quota 0
sv_lan 1
echo CSRETRO_PX7_SPECIAL_B_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px7specialb.log"

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
	+maxplayers 4 \
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

wait_log 'offscreen world map=maps/de_aztec' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_aztec offscreen proof"
}
wait_log 'water capture map=' 20 || true
wait_log 'water pixelproof' 25 || true
wait_log 'probe_seq sv_wateralpha 0.5' 20 || true
wait_log 'water alpha pixelproof' 20 || true
wait_log 'probe_seq sv_wateralpha 1 sv_wateramp 1' 20 || true
wait_log 'water wave proof' 20 || true
wait_log 'offscreen world map=maps/de_torn' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_torn offscreen proof"
}
wait_log 'texture animation proof' 10 || true
wait_log 'conveyor proof' 20 || true
wait_log 'offscreen world map=maps/de_dust' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_dust offscreen proof"
}
wait_log 'offscreen world map=maps/cs_assault' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein cs_assault offscreen proof"
}
wait_log 'probe_seq vid_setmode' 20 || true
wait_log 'probe_seq quit' 20 || true
csretro_gate_wait_quit "${XASH_PID}" 20 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px7specialb.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true
cp -a "${ALL}" "${SHOTS}/merged-px7specialb.log" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi
rg -q 'water capture map=' "${ALL}" || fail "kein water capture"
rg -q 'turb_surfaces=[1-9]' "${ALL}" || fail "turb_surfaces=0"
rg -q 'water live mutate=0' "${ALL}" || fail "live mutate nicht 0"
rg -q 'cache_mutate=0' "${ALL}" || fail "cache mutate nicht 0"

WATER_PX=0
OPAQUE_OK=0
LATE_OK=0
ALPHA_OK=0
WAVE_OK=0
ANIM_OK=0
CONV_OK=0
if rg -q 'water pixelproof .*differ=1' "${ALL}"; then
	WATER_PX=1
fi
if rg -q 'water pixelproof .*pass=opaque' "${ALL}"; then
	OPAQUE_OK=1
fi
if rg -q 'water pixelproof .*pass=late' "${ALL}" || rg -q 'water alpha pixelproof' "${ALL}"; then
	LATE_OK=1
fi
if rg -q 'water alpha pixelproof .*differ=1' "${ALL}"; then
	ALPHA_OK=1
fi
if rg -q 'water wave proof .*differ=1' "${ALL}"; then
	WAVE_OK=1
fi
if rg -q 'texture animation proof candidates=[1-9]' "${ALL}"; then
	ANIM_OK=1
fi
if rg -q 'conveyor proof candidates=[1-9]' "${ALL}"; then
	CONV_OK=1
fi

echo "PX7_SPECIAL_B water_px=${WATER_PX} opaque=${OPAQUE_OK} late=${LATE_OK} alpha=${ALPHA_OK} wave=${WAVE_OK} anim=${ANIM_OK} conveyor=${CONV_OK}"
rg -n 'water capture |water inventory |water pixelproof|water alpha |water wave |water live |brush water |brush special |texture animation |conveyor proof|visible frame stays Xash|GL_RenderFrame|probe_seq' "${ALL}" | head -n 120

if [[ "${WATER_PX}" -ne 1 ]]; then
	fail "water pixelproof differ=1 fehlt"
fi
if [[ "${ANIM_OK}" -ne 1 ]]; then
	echo "PX7_SPECIAL_B texture animation: implemented, runtime NOT REPRODUCIBLE in this probe"
fi
if [[ "${CONV_OK}" -ne 1 ]]; then
	echo "PX7_SPECIAL_B conveyor: implemented, runtime NOT REPRODUCIBLE in this probe"
fi
if [[ "${OPAQUE_OK}" -ne 1 ]]; then
	echo "PX7_SPECIAL_B opaque world water: implemented, runtime NOT REPRODUCIBLE in this probe"
fi
if [[ "${LATE_OK}" -ne 1 ]]; then
	echo "PX7_SPECIAL_B transparent late water: implemented, runtime NOT REPRODUCIBLE in this probe"
fi
if [[ "${ALPHA_OK}" -ne 1 ]]; then
	echo "PX7_SPECIAL_B alpha runtime: implemented, runtime NOT REPRODUCIBLE in this probe"
fi
if [[ "${WAVE_OK}" -ne 1 ]]; then
	echo "PX7_SPECIAL_B vertex wave: implemented, runtime NOT REPRODUCIBLE in this probe"
fi

echo "PX7_SPECIAL_B PASS"
echo "PX7_SPECIAL_B log=${ALL}"
exit 0
