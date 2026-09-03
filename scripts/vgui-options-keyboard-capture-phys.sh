#!/usr/bin/env bash
# Physical capture path: SDL → Key_Event → UI_KeyEvent → VGuiXash_Key → FinishCapture
# Proves F8 and printable 'q' WHILE capturing=1 (GetExtAPI / no text-input swallow).
# Usage: ./scripts/build-menu.sh && ./scripts/vgui-options-keyboard-capture-phys.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-isolate.sh"

ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
W="${CSRETRO_GATE_W:-640}"
H="${CSRETRO_GATE_H:-480}"
cd "${ROOT}"

fail() { echo "KEYBOARD_CAPTURE_PHYS FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
MENU="$(readlink -f "${MENU}")"
command -v xdotool >/dev/null 2>&1 || fail "xdotool fehlt"
command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

export CSRETRO_ENGINE_OUT="${ENG}" CSRETRO_CLIENT_SO="${CLIENT}" \
	CSRETRO_GAMEDLL_SO="${GAMEDLL}" CSRETRO_MENU_SO="${MENU}"
csretro_gate_isolate_begin "keyboard-phys" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"

cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
echo CSRETRO_OPTIONS_KEYBOARD_PHYS_CFG
EOF

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
csretro_gate_isolate_note
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS=1
export CSRETRO_KEYBOARD_CAPTURE_DEBUG=1
export CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
export CSRETRO_MENU_SHA256="$(sha256sum "${MENU}" | awk '{print $1}')"
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE \
	CSRETRO_OPTIONS_AUDIO_GATE CSRETRO_OPTIONS_KEYBOARD_GATE \
	CSRETRO_OPTIONS_VIDEO_GATE CSRETRO_OPTIONS_VIDEO_MODE_SAFETY 2>/dev/null || true
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_W="${W}"
export CSRETRO_GAMESCOPE_H="${H}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-keyboard-phys.log"

find_wid() {
	local wid="" i
	for i in $(seq 1 40); do
		wid="$(xdotool search --name 'CS Retro' 2>/dev/null | head -n1 || true)"
		[[ -z "${wid}" ]] && wid="$(xdotool search --class 'xash' 2>/dev/null | head -n1 || true)"
		[[ -z "${wid}" ]] && wid="$(xdotool search --class 'SDL' 2>/dev/null | head -n1 || true)"
		[[ -n "${wid}" ]] && { echo "${wid}"; return 0; }
		sleep 0.25
	done
	return 1
}

log_has() {
	local pat="$1"
	rg -q "${pat}" "${LOG}" 2>/dev/null || rg -q "${pat}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null
}

wait_log() {
	local pat="$1" secs="$2" i
	for i in $(seq 1 "${secs}"); do
		if log_has "${pat}"; then
			return 0
		fi
		sleep 0.5
	done
	return 1
}

# Count matching lines across both logs (for "new event after marker" checks).
count_pat() {
	local pat="$1"
	{ rg -c "${pat}" "${LOG}" 2>/dev/null || true
	  rg -c "${pat}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null || true
	} | awk '{s+=$1} END {print s+0}'
}

stop_engine() {
	local pid="${1:-}"
	[[ -n "${pid}" ]] && kill "${pid}" 2>/dev/null || true
	csretro_headless_x11_stop
	killall -q xash3d 2>/dev/null || true
	sleep 0.3
}

dump_kb() {
	rg -n 'CSRETRO_KB|PHYS|finish |VGuiXash_Key|BeginCapture|extApi' \
		"${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | tail -80 >&2 || true
}

csretro_headless_x11_prepare || fail "gamescope"
rm -f "${LOG}"
: >"${CSRETRO_GAMESCOPE_LOG}"
killall -q xash3d 2>/dev/null || true
sleep 0.2

cd "${RUN}"
set +e
csretro_headless_x11_wrap ./xash3d \
	-game cstrike \
	-dll "${GAMEDLL}" \
	-clientlib "${CLIENT}" \
		-menulib "${MENU}" \
	-windowed -width "${W}" -height "${H}" \
	-dev 2 -log \
	+maxplayers 2 +sv_lan 1 +exec autoexec.cfg \
	>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
XASH_PID=$!
CSRETRO_GAMESCOPE_PID="${XASH_PID}"
set -e
cd "${ROOT}"

csretro_headless_x11_wait_display || { stop_engine "${XASH_PID}"; fail "X11"; }

wait_log 'CSRETRO_OPTIONS_VISIBLE' 40 || { stop_engine "${XASH_PID}"; fail "VISIBLE"; }
wait_log 'extApi=1' 20 || { stop_engine "${XASH_PID}"; fail "extApi!=1"; }

# --- Phase 1: F8 while capturing ---
wait_log 'CSRETRO_KEYBOARD_PHYS_WAIT key=F8' 30 || { dump_kb; stop_engine "${XASH_PID}"; fail "PHYS_WAIT F8"; }

WID="$(find_wid || true)"
[[ -n "${WID}" ]] || { stop_engine "${XASH_PID}"; fail "no window for xdotool"; }

xdotool key --window "${WID}" F8 >/dev/null 2>&1 || true

if ! wait_log 'VGuiXash_Key key=142 down=1 .*capturing=1' 10; then
	dump_kb
	stop_engine "${XASH_PID}"
	fail "F8: no VGuiXash_Key key=142 capturing=1"
fi
if ! wait_log 'finish keynum=142 name=F8' 10; then
	dump_kb
	stop_engine "${XASH_PID}"
	fail "F8: no finish keynum=142 name=F8"
fi
echo "KEYBOARD_CAPTURE_PHYS OK F8 via SDL→VGuiXash_Key while capturing"

# --- Phase 2: printable q WHILE capturing (GetExtAPI claim) ---
wait_log 'CSRETRO_KEYBOARD_PHYS_WAIT key=q' 20 || { dump_kb; stop_engine "${XASH_PID}"; fail "PHYS_WAIT q (re-arm)"; }

# Snapshot counts so we require *new* events after re-arm (not leftover F8-era noise).
BEFORE_113="$(count_pat 'VGuiXash_Key key=113 down=1 .*capturing=1')"
BEFORE_FINISH_Q="$(count_pat 'finish keynum=113')"

xdotool key --window "${WID}" q >/dev/null 2>&1 || true

ok_113=0
ok_finish=0
for _ in $(seq 1 20); do
	cur_113="$(count_pat 'VGuiXash_Key key=113 down=1 .*capturing=1')"
	cur_fin="$(count_pat 'finish keynum=113')"
	[[ "${cur_113}" -gt "${BEFORE_113}" ]] && ok_113=1
	# name=q (Xash KeynumToString) — accept name=Q too
	if rg -q 'finish keynum=113 name=[qQ]' "${LOG}" 2>/dev/null || \
	   rg -q 'finish keynum=113 name=[qQ]' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
		[[ "${cur_fin}" -gt "${BEFORE_FINISH_Q}" ]] && ok_finish=1
	fi
	if [[ "${ok_113}" -eq 1 && "${ok_finish}" -eq 1 ]]; then
		break
	fi
	sleep 0.5
done

if [[ "${ok_113}" -ne 1 ]]; then
	dump_kb
	stop_engine "${XASH_PID}"
	fail "letter q: no VGuiXash_Key key=113 down=1 capturing=1 (GetExtAPI printable path)"
fi
if [[ "${ok_finish}" -ne 1 ]]; then
	dump_kb
	stop_engine "${XASH_PID}"
	fail "letter q: no finish keynum=113 while/after capture"
fi
echo "KEYBOARD_CAPTURE_PHYS OK letter q keynum=113 finish while capturing (GetExtAPI)"

wait_log 'CSRETRO_KEYBOARD_PHYS_DONE' 10 || true

stop_engine "${XASH_PID}"
echo "KEYBOARD_CAPTURE_PHYS AUTOMATED_OK"
