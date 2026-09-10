#!/usr/bin/env bash
# Native Sichtpruefung fuer Molotov: kaufen, anzuenden/halten, werfen.
# Laeuft in einem isolierten BASEDIR und veraendert keine Benutzerkonfiguration.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/gamedata-env.sh
source "${ROOT}/scripts/gamedata-env.sh"
# shellcheck source=scripts/headless-x11.sh
source "${ROOT}/scripts/headless-x11.sh"
# shellcheck source=scripts/gate-isolate.sh
source "${ROOT}/scripts/gate-isolate.sh"

ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
MAP="${CSRETRO_FIRE_MAP:-de_dust}"
W="${CSRETRO_FIRE_W:-1280}"
H="${CSRETRO_FIRE_H:-720}"
SHOT_DIR="${ROOT}/build/fire-shots"
TEAM="${CSRETRO_FIRE_TEAM:-t}"
if [[ "${TEAM}" == "ct" ]]; then
	TEAM_KEY=2
	GRENADE_CMD=incgrenade
	SHOT_TAG=incendiary
else
	TEAM_KEY=1
	GRENADE_CMD=molotov
	SHOT_TAG=molotov
fi

fail() { echo "FIRE_GRENADE_GATE FAIL: $*" >&2; exit 1; }
command -v xdotool >/dev/null 2>&1 || fail "xdotool fehlt"
command -v import >/dev/null 2>&1 || fail "ImageMagick import fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" && -f "${GAMEDLL}" && -f "${MENU}" ]] || fail "Binaries fehlen"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

CSRETRO_ENGINE_OUT="$(readlink -f "${ENG}")"
CSRETRO_CLIENT_SO="$(readlink -f "${CLIENT}")"
CSRETRO_GAMEDLL_SO="$(readlink -f "${GAMEDLL}")"
CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
export CSRETRO_ENGINE_OUT CSRETRO_CLIENT_SO CSRETRO_GAMEDLL_SO CSRETRO_MENU_SO
csretro_gate_isolate_begin "fire-grenade" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
mkdir -p "${SHOT_DIR}"

apply_cfg() {
	local target="$1"
	{
		echo 'developer 2'
		echo 'mp_auto_join_team 0'
		echo 'mp_limitteams 0'
		echo 'mp_autoteambalance 0'
		echo 'mp_round_infinite 1'
		echo 'mp_buytime 9'
		echo 'mp_freezetime 0'
		echo 'mp_buy_anywhere 1'
		echo '_vgui_menus 1'
		echo "bind \"F6\" \"${GRENADE_CMD}; weapon_${GRENADE_CMD}\""
		echo 'bind "F7" "+attack"'
		echo 'bind "F8" "-attack"'
		echo 'echo CSRETRO_FIRE_GATE_CFG'
	} >"${target}"
}
apply_cfg "${RUN}/cstrike/autoexec.cfg"
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/listenserver.cfg"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' >"${RUN}/cstrike/cstrike.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' >"${RUN}/valve/valve.rc"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_GAMESCOPE_W="${W}" CSRETRO_GAMESCOPE_H="${H}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope.log"
export CSRETRO_RUN_DIR="${RUN}"

stop_run() {
	if [[ -n "${XASH_PID:-}" ]]; then
		kill -TERM "${XASH_PID}" 2>/dev/null || true
		wait "${XASH_PID}" 2>/dev/null || true
	fi
	csretro_headless_x11_stop
}
trap stop_run EXIT

csretro_headless_x11_prepare || fail "gamescope"
cd "${RUN}"
csretro_headless_x11_wrap ./xash3d -game cstrike \
	-dll "${GAMEDLL}" -clientlib "${CLIENT}" -menulib "${MENU}" \
	-windowed -width "${W}" -height "${H}" -dev 2 -log \
	+maxplayers 10 +sv_lan 1 +map "${MAP}" +exec autoexec.cfg \
	>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
XASH_PID=$!
# Used by csretro_headless_x11_stop from the sourced helper.
# shellcheck disable=SC2034
CSRETRO_GAMESCOPE_PID="${XASH_PID}"
cd "${ROOT}"
csretro_headless_x11_wait_display || fail "X11 nicht bereit"

WID=""
for _ in $(seq 1 100); do
	WID="$(xdotool search --onlyvisible --name 'CS Retro' 2>/dev/null | head -n1 || true)"
	[[ -n "${WID}" ]] && break
	sleep 0.1
done
[[ -n "${WID}" ]] || fail "Spielfenster fehlt"

sleep 2
xdotool key --window "${WID}" "${TEAM_KEY}" >/dev/null 2>&1
sleep 1
xdotool key --window "${WID}" 1 >/dev/null 2>&1
sleep 2
xdotool key --window "${WID}" F6 >/dev/null 2>&1
sleep 0.25
import -window "${WID}" "${SHOT_DIR}/${SHOT_TAG}-deploy.png"
sleep 0.75
xdotool keydown --window "${WID}" F7 >/dev/null 2>&1
sleep 0.40
import -window "${WID}" "${SHOT_DIR}/${SHOT_TAG}-pinpull.png"
sleep 0.45
import -window "${WID}" "${SHOT_DIR}/${SHOT_TAG}-held.png"
xdotool keyup --window "${WID}" F7 >/dev/null 2>&1 || true
xdotool key --window "${WID}" F8 >/dev/null 2>&1 || true
sleep 0.18
import -window "${WID}" "${SHOT_DIR}/${SHOT_TAG}-throw.png"
sleep 2.32
import -window "${WID}" "${SHOT_DIR}/${SHOT_TAG}-after-throw.png"

echo "FIRE_GRENADE_GATE PASS team=${TEAM} deploy/pin/held/throw/after=${SHOT_DIR}/${SHOT_TAG}-*.png"
