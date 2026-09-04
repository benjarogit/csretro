#!/usr/bin/env bash
# Windowed console: configurable bind -> VGUI input -> engine command -> close/persist.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-isolate.sh"

ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${CSRETRO_SMOKE_MAP:-de_dust}")"

fail() { echo "VGUI_CONSOLE_GATE FAIL: $*" >&2; exit 1; }
command -v xdotool >/dev/null 2>&1 || fail "xdotool fehlt"
command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${MENU}" ]] || fail "Menü fehlt"

export CSRETRO_ENGINE_OUT="${ENG}" CSRETRO_CLIENT_SO="${CLIENT}" \
	CSRETRO_GAMEDLL_SO="${GAMEDLL}" CSRETRO_MENU_SO="${MENU}"
csretro_gate_isolate_begin "console" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
OUT="${RUN}/gamescope-console-gate.log"
SHOT_DIR="${ROOT}/build/console-shots"
mkdir -p "${SHOT_DIR}"

cat > "${RUN}/cstrike/config.cfg" <<'EOF'
unbindall
bind "F6" "toggleconsole"
bind "ESCAPE" "cancelselect"
developer "2"
EOF
printf '%s\n' 'exec config.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_CONSOLE_DEBUG=1
export CSRETRO_GAMESCOPE_W="${CSRETRO_GATE_W:-800}"
export CSRETRO_GAMESCOPE_H="${CSRETRO_GATE_H:-600}"
export CSRETRO_GAMESCOPE_LOG="${OUT}"

log_has() { rg -q "$1" "${LOG}" "${OUT}" 2>/dev/null; }
wait_log() {
	local pattern="$1" count="${2:-80}"
	for _ in $(seq 1 "${count}"); do
		log_has "${pattern}" && return 0
		sleep 0.25
	done
	return 1
}
stop_all() {
	[[ -n "${XASH_PID:-}" ]] && kill "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	killall -q xash3d 2>/dev/null || true
}
trap stop_all EXIT

csretro_headless_x11_prepare || fail "X11"
rm -f "${LOG}" "${OUT}"
: > "${OUT}"

cd "${RUN}"
csretro_headless_x11_wrap ./xash3d \
	-game cstrike -dll "${GAMEDLL}" -clientlib "${CLIENT}" -menulib "${MENU}" \
	-windowed -width "${CSRETRO_GAMESCOPE_W}" -height "${CSRETRO_GAMESCOPE_H}" \
	-dev 2 -log +maxplayers 2 +sv_lan 1 >>"${OUT}" 2>&1 &
XASH_PID=$!
cd "${ROOT}"

csretro_headless_x11_wait_display || fail "Display nicht bereit"
wait_log 'VGUI Xash runtime initialized' 160 || fail "VGUI nicht initialisiert"

WID=""
for _ in $(seq 1 80); do
	WID="$(xdotool search --name 'CS Retro' 2>/dev/null | head -n1 || true)"
	[[ -z "${WID}" ]] && WID="$(xdotool search --class 'xash' 2>/dev/null | head -n1 || true)"
	[[ -n "${WID}" ]] && break
	sleep 0.25
done
[[ -n "${WID}" ]] || fail "Spielfenster nicht gefunden"

xdotool key --window "${WID}" F6
wait_log 'CSRETRO_CONSOLE_OPEN' || fail "F6/toggleconsole öffnet nicht"

xdotool type --window "${WID}" --delay 12 'echo CSRETRO_CONSOLE_COMMAND_OK'
xdotool key --window "${WID}" Return
for _ in $(seq 1 80); do
	rg -q '^CSRETRO_CONSOLE_COMMAND_OK[[:space:]]*$' "${LOG}" 2>/dev/null && break
	sleep 0.25
done
rg -q '^CSRETRO_CONSOLE_COMMAND_OK[[:space:]]*$' "${LOG}" 2>/dev/null \
	|| fail "Konsoleneingabe wurde nicht vom Engine-Parser ausgeführt"

xdotool type --window "${WID}" --delay 12 'screenshot'
xdotool key --window "${WID}" Return
sleep 1
NEWEST="$(find "${RUN}/cstrike/scrshots" -maxdepth 2 \( -name '*.png' -o -name '*.bmp' \) -size +20k \
	-printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n1 | cut -d' ' -f2- || true)"
[[ -n "${NEWEST}" && -f "${NEWEST}" ]] || fail "Konsolen-Screenshot fehlt"
SHOT="${SHOT_DIR}/csretro-console-800x600.${NEWEST##*.}"
cp -a "${NEWEST}" "${SHOT}"

xdotool key --window "${WID}" F6
wait_log 'CSRETRO_CONSOLE_CLOSE' || fail "F6/toggleconsole schließt nicht"
rg -q '^Console ' "${RUN}/cfg/csretro_ui_geometry.txt" || fail "Console-Geometrie nicht gespeichert"

echo "VGUI_CONSOLE_GATE AUTOMATED_OK bind=F6 command=OK geometry=OK shot=${SHOT}"
