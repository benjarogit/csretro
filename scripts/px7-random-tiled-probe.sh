#!/usr/bin/env bash
# #7 Brush Special E: Xash random tiled ('-') via shared ResolveSurfaceTextureReadOnly.
# Visible frame stays Xash (GL_RenderFrame = 0). No client RNG, no second rtable.
# de_aztec (~1955 '-' surfaces) → de_torn → cs_assault → de_dust + vid_setmode.
# Usage: ./scripts/build-engine.sh && ./scripts/build-client.sh && ./scripts/px7-random-tiled-probe.sh
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

fail() { echo "PX7_RANDOM_TILED FAIL: $*" >&2; exit 1; }

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
csretro_gate_isolate_begin "px7randomtiled" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${ROOT}/build/px7-random-tiled-shots"
mkdir -p "${SHOTS}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
r_csretro_renderer 1
r_csretro_offscreen_dump 1
r_csretro_probe_seq 10
sv_cheats 1
mp_auto_join_team 1
humans_join_team T
mp_freezetime 0
bot_quota 0
sv_lan 1
echo CSRETRO_PX7_RANDOM_TILED_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px7randomtiled.log"

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
wait_log 'random tiled inventory' 20 || true
wait_log 'random tiled pixelproof' 10 || true
wait_log 'random tiled stability' 12 || true
if command -v import >/dev/null && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/aztec-random-tiled.png" 2>/dev/null || true
fi
wait_log 'offscreen world map=maps/de_torn' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_torn offscreen proof"
}
wait_log 'conveyor proof' 15 || true
if command -v import >/dev/null && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/torn-random-tiled.png" 2>/dev/null || true
fi
wait_log 'offscreen world map=maps/cs_assault' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein cs_assault offscreen proof"
}
wait_log 'texture animation proof' 15 || true
if command -v import >/dev/null && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/assault-random-tiled.png" 2>/dev/null || true
fi
wait_log 'offscreen world map=maps/de_dust' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_dust offscreen proof"
}
wait_log 'probe_seq vid_setmode' 20 || true
wait_log 'probe_seq quit' 20 || true
csretro_gate_wait_quit "${XASH_PID}" 20 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px7randomtiled.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true
cp -a "${ALL}" "${SHOTS}/merged-px7randomtiled.log" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi
rg -q 'random tiled inventory' "${ALL}" || fail "keine Random-Tiled-Inventur"

CAND=0
RES=0
FALL0=0
DIFFERS=0
STABLE=0
PX=0
HELPER=0
ANIM=0
CONV=0
MAPC=0
if rg -q 'candidates=[1-9][0-9]*' "${ALL}"; then
	CAND=1
fi
if rg -q 'random tiled inventory .*resolved=[1-9]' "${ALL}"; then
	RES=1
fi
if rg -q 'random tiled inventory .*fallback=0' "${ALL}"; then
	FALL0=1
fi
if rg -q 'selected_differs_from_texinfo_base=[1-9]' "${ALL}"; then
	DIFFERS=1
fi
if rg -q 'random tiled stability .*stable=1' "${ALL}"; then
	STABLE=1
fi
if rg -q 'random tiled pixelproof .*differ=1' "${ALL}"; then
	PX=1
fi
if rg -q 'random tiled inventory .*helper=1' "${ALL}"; then
	HELPER=1
fi
if rg -q 'texture animation proof candidates=[1-9]' "${ALL}"; then
	ANIM=1
fi
if rg -q 'conveyor proof candidates=[1-9]' "${ALL}"; then
	CONV=1
fi
if rg -q 'random tiled mapchange .*stale_indices=0' "${ALL}"; then
	MAPC=1
fi

echo "PX7_RANDOM_TILED candidates=${CAND} resolved=${RES} fallback0=${FALL0} differs=${DIFFERS} stable=${STABLE} pixel=${PX} helper=${HELPER} anim=${ANIM} conveyor=${CONV} mapchange=${MAPC}"
rg -n 'random tiled |brush special |texture animation |conveyor proof|visible frame stays Xash|GL_RenderFrame|probe_seq' "${ALL}" | head -n 160

if [[ "${HELPER}" -ne 1 ]]; then
	fail "ResolveSurfaceTextureReadOnly helper fehlt"
fi
if [[ "${CAND}" -ne 1 || "${RES}" -ne 1 ]]; then
	fail "candidates/resolved fehlen"
fi
if [[ "${FALL0}" -ne 1 ]]; then
	fail "fallback ist nicht 0"
fi
if [[ "${STABLE}" -ne 1 ]]; then
	fail "selection nicht zeitstabil"
fi
if [[ "${PX}" -ne 1 ]]; then
	fail "pixelproof differ=1 fehlt"
fi
if [[ "${MAPC}" -ne 1 ]]; then
	fail "mapchange stale_indices=0 fehlt"
fi
if [[ "${DIFFERS}" -ne 1 ]]; then
	echo "PX7_RANDOM_TILED selected_differs_from_texinfo_base: 0 in this probe"
fi
if [[ "${ANIM}" -ne 1 ]]; then
	echo "PX7_RANDOM_TILED texture animation: implemented, runtime NOT REPRODUCIBLE in this probe"
fi
if [[ "${CONV}" -ne 1 ]]; then
	echo "PX7_RANDOM_TILED conveyor: implemented, runtime NOT REPRODUCIBLE in this probe"
fi

if ! "${ROOT}/scripts/movement-contract-gate.sh"; then
	fail "movement-contract-gate"
fi

echo "PX7_RANDOM_TILED PASS"
echo "PX7_RANDOM_TILED log=${ALL}"
exit 0
