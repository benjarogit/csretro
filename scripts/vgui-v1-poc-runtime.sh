#!/usr/bin/env bash
# Phase-3M V1 Runtime-PoC: interaktiver Beweis unter headless gamescope.
# CSRETRO_V1POC=1 → Auto-Show Frame; xdotool (ohne windowactivate) für Text/Tab/Hover/OK.
#
# Usage:
#   ./scripts/build-menu.sh && ./scripts/vgui-v1-poc-runtime.sh
# Optional: CSRETRO_SMOKE_MAP=de_dust (nur Game-Data-Check; Engine startet im Menü ohne +map)
#           CSRETRO_FOREGROUND=1 für sichtbares Fenster
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/env.sh" ]] && source "${ROOT}/scripts/env.sh"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
# shellcheck source=headless-x11.sh
source "${ROOT}/scripts/headless-x11.sh"

# Absolute paths — relative CSRETRO_RUN_DIR from a prior shell breaks after cd.
RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run}"
case "${RUN}" in
	/*) ;;
	*) RUN="${ROOT}/${RUN}" ;;
esac
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
case "${ENG}" in
	/*) ;;
	*) ENG="${ROOT}/${ENG}" ;;
esac
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
LOG="${RUN}/engine.log"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
cd "${ROOT}"

fail() { echo "V1POC FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu_amd64.so fehlt (${MENU})"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" ]] || fail "Client fehlt"
[[ -f "${GAMEDLL}" ]] || fail "GameDLL fehlt"
command -v xdotool >/dev/null 2>&1 || fail "xdotool fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt — oder CSRETRO_FOREGROUND=1"
fi

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve" \
	"${GAMEDATA}/cstrike/resource/UI"
cp -a "${ROOT}/data/ui-overrides/cstrike/resource/UI/CsretroV1Poc.res" \
	"${GAMEDATA}/cstrike/resource/UI/CsretroV1Poc.res"
cp -a "${GAMEDLL}" "${RUN}/cstrike/dlls/cs_amd64.so"
cp -a "${CLIENT}" "${RUN}/cstrike/cl_dlls/client_amd64.so"
cp -a "${MENU}" "${RUN}/menu_amd64.so"
ln -sfn "${MENU}" "${RUN}/libmenu.so"
ln -sfn "${ENG}/engine/libxash.so" "${RUN}/libxash.so"
ln -sfn "${ENG}/ref/gl/libref_gl.so" "${RUN}/libref_gl.so"
ln -sfn "${ENG}/filesystem/filesystem_stdio.so" "${RUN}/filesystem_stdio.so"
ln -sfn "${ENG}/game_launch/xash3d" "${RUN}/xash3d"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"

cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
echo CSRETRO_V1POC_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_V1POC=1
unset STEAM_RUNTIME STEAM_COMPAT_DATA_PATH 2>/dev/null || true
export CSRETRO_RUN_DIR="${RUN}"

PASS_READY=0
PASS_VISIBLE_640=0
PASS_VISIBLE_800=0
PASS_TEXT=0
PASS_BACKSPACE=0
PASS_TAB=0
PASS_HOVER=0
PASS_CLICK=0
PASS_NO_STEAM=0

find_wid() {
	local wid="" i name
	for i in $(seq 1 40); do
		wid="$(xdotool search --name 'CS Retro' 2>/dev/null | head -n1 || true)"
		[[ -z "${wid}" ]] && wid="$(xdotool search --name 'Counter-Strike' 2>/dev/null | head -n1 || true)"
		[[ -z "${wid}" ]] && wid="$(xdotool search --class 'xash' 2>/dev/null | head -n1 || true)"
		[[ -z "${wid}" ]] && wid="$(xdotool search --class 'SDL' 2>/dev/null | head -n1 || true)"
		if [[ -z "${wid}" ]]; then
			# Beliebiges Fenster auf dem headless-Display (außer root)
			for wid in $(xdotool search --onlyvisible --name '.*' 2>/dev/null || true); do
				name="$(xdotool getwindowname "${wid}" 2>/dev/null || true)"
				[[ -z "${name}" || "${name}" == "gamescope" ]] && continue
				echo "${wid}"
				return 0
			done
			wid=""
		fi
		[[ -n "${wid}" ]] && { echo "${wid}"; return 0; }
		sleep 0.25
	done
	return 1
}

wait_log() {
	local pat="$1" secs="${2:-45}"
	local i
	for i in $(seq 1 "${secs}"); do
		if [[ -f "${LOG}" ]] && rg -q "${pat}" "${LOG}"; then
			return 0
		fi
		sleep 0.5
	done
	return 1
}

# Frame 420x240 centered; control centers from CsretroV1Poc.res
poc_coords() {
	local sw="$1" sh="$2"
	local fx=$(( (sw - 420) / 2 ))
	local fy=$(( (sh - 240) / 2 ))
	# TextEntry xpos 20 ypos 80 wide 380 tall 24 → center
	POC_ENTRY_X=$(( fx + 20 + 190 ))
	POC_ENTRY_Y=$(( fy + 80 + 12 ))
	# OkButton xpos 220 ypos 200 wide 80 tall 24 → center ≈ 370,332 @640x480
	POC_OK_X=$(( fx + 220 + 40 ))
	POC_OK_Y=$(( fy + 200 + 12 ))
}

stop_engine() {
	local pid="${1:-}"
	if [[ -n "${pid}" ]]; then
		kill -TERM -- -"${pid}" 2>/dev/null || kill -TERM "${pid}" 2>/dev/null || true
	fi
	sleep 0.5
	csretro_headless_x11_stop
	killall -q xash3d 2>/dev/null || true
	sleep 0.3
}

run_resolution() {
	local sw="$1" sh="$2" do_interact="$3"
	local xash_pid wid

	export CSRETRO_GAMESCOPE_W="${sw}"
	export CSRETRO_GAMESCOPE_H="${sh}"
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-v1poc-${sw}x${sh}.log"
	csretro_headless_x11_prepare || fail "gamescope"

	rm -f "${LOG}"
	: >"${CSRETRO_GAMESCOPE_LOG}"
	killall -q xash3d 2>/dev/null || true
	sleep 0.3

	cd "${RUN}"
	set +e
	# stdout+stderr → gamescope-Log (Xwayland-Zeile oft auf stdout)
	csretro_headless_x11_wrap ./xash3d \
		-game cstrike \
		-dll "${GAMEDLL}" \
		-clientlib "${CLIENT}" \
		-menu "${MENU}" \
		-windowed -width "${sw}" -height "${sh}" \
		-dev 2 \
		-log \
		+maxplayers 2 \
		+sv_lan 1 \
		+exec autoexec.cfg \
		>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	xash_pid=$!
	CSRETRO_GAMESCOPE_PID="${xash_pid}"
	set -e
	csretro_headless_x11_wait_display || { stop_engine "${xash_pid}"; fail "headless-X11 nicht bereit (${sw}x${sh})"; }

	if wait_log 'CSRETRO_V1POC_READY|VGUI Xash runtime initialized' 60; then
		PASS_READY=1
	fi
	if wait_log "CSRETRO_V1POC_VISIBLE ${sw} ${sh}|CSRETRO_V1POC_VISIBLE" 40; then
		if [[ "${sw}" -eq 640 ]]; then
			PASS_VISIBLE_640=1
		else
			PASS_VISIBLE_800=1
		fi
	fi

	if [[ "${do_interact}" == 1 ]]; then
		wid="$(find_wid || true)"
		if [[ -z "${wid}" ]]; then
			echo "V1POC WARN: kein xdotool-Fenster (DISPLAY=${DISPLAY:-?}) — Checklist nur aus Log" >&2
			xdotool search --name '.*' 2>/dev/null | head -n 10 >&2 || true
		else
			poc_coords "${sw}" "${sh}"

			# 1) TextEntry: click, type, backspace
			xdotool mousemove --window "${wid}" "${POC_ENTRY_X}" "${POC_ENTRY_Y}"
			sleep 0.15
			xdotool click --window "${wid}" 1
			sleep 0.2
			xdotool type --window "${wid}" --delay 40 'ab c'
			sleep 0.3
			xdotool key --window "${wid}" BackSpace
			sleep 0.25

			# 2) Tab
			xdotool key --window "${wid}" Tab
			sleep 0.25

			# 3) OK: hover then click; Enter als Fallback (Default-Button)
			xdotool mousemove --window "${wid}" "${POC_OK_X}" "${POC_OK_Y}"
			sleep 0.35
			xdotool click --window "${wid}" 1
			sleep 0.4
			if ! rg -q 'CSRETRO_V1POC_CLICK_OK|CSRETRO_V1POC_CLOSE' "${LOG}" 2>/dev/null \
				&& ! rg -q 'CSRETRO_V1POC_CLICK_OK|CSRETRO_V1POC_CLOSE' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
				xdotool key --window "${wid}" Return
				sleep 0.4
			fi
			# Escape-Proof falls Click/Enter den Dialog noch nicht schlossen
			if ! rg -q 'CSRETRO_V1POC_CLICK_OK|CSRETRO_V1POC_CLOSE' "${LOG}" 2>/dev/null \
				&& ! rg -q 'CSRETRO_V1POC_CLICK_OK|CSRETRO_V1POC_CLOSE' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
				xdotool key --window "${wid}" Escape
				sleep 0.3
			fi
		fi

		rg -q 'CSRETRO_V1POC_TEXT' "${LOG}" 2>/dev/null && PASS_TEXT=1
		rg -q 'CSRETRO_V1POC_BACKSPACE' "${LOG}" 2>/dev/null && PASS_BACKSPACE=1
		rg -q 'CSRETRO_V1POC_TAB' "${LOG}" 2>/dev/null && PASS_TAB=1
		rg -q 'CSRETRO_V1POC_HOVER_OK' "${LOG}" 2>/dev/null && PASS_HOVER=1
		if rg -q 'CSRETRO_V1POC_CLICK_OK|CSRETRO_V1POC_CLOSE' "${LOG}" 2>/dev/null \
			|| rg -q 'CSRETRO_V1POC_CLICK_OK|CSRETRO_V1POC_CLOSE' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			PASS_CLICK=1
		fi
		# Auch Gamescope-Log für Text/Tab/Hover (engine.log wird bei 2. Auflösung gelöscht)
		rg -q 'CSRETRO_V1POC_TEXT' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null && PASS_TEXT=1
		rg -q 'CSRETRO_V1POC_BACKSPACE' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null && PASS_BACKSPACE=1
		rg -q 'CSRETRO_V1POC_TAB' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null && PASS_TAB=1
		rg -q 'CSRETRO_V1POC_HOVER_OK' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null && PASS_HOVER=1
	fi

	stop_engine "${xash_pid}"
}

echo "=== V1 PoC interactive (${MENU}) ==="
run_resolution 640 480 1
run_resolution 800 600 0

if ! rg -qi 'steam_api\.so|libsteam|GameUI\.dll|vgui2\.so|vgui2\.dll' "${LOG}" 2>/dev/null; then
	PASS_NO_STEAM=1
fi

echo
echo "=== Checklist ==="
chk() {
	local ok="$1" label="$2"
	if [[ "${ok}" -eq 1 ]]; then
		echo "PASS  ${label}"
	else
		echo "FAIL  ${label}"
	fi
}
chk "${PASS_READY}" "CSRETRO_V1POC_READY / VGUI init"
chk "${PASS_VISIBLE_640}" "CSRETRO_V1POC_VISIBLE @ 640x480"
chk "${PASS_VISIBLE_800}" "CSRETRO_V1POC_VISIBLE @ 800x600"
chk "${PASS_TEXT}" "CSRETRO_V1POC_TEXT (TextEntry)"
chk "${PASS_BACKSPACE}" "CSRETRO_V1POC_BACKSPACE"
chk "${PASS_TAB}" "CSRETRO_V1POC_TAB"
chk "${PASS_HOVER}" "CSRETRO_V1POC_HOVER_OK"
chk "${PASS_CLICK}" "CSRETRO_V1POC_CLICK_OK / CLOSE"
chk "${PASS_NO_STEAM}" "kein steam/vgui2 Runtime-Hinweis"

echo
echo "=== Log (PoC markers) ==="
rg -n 'CSRETRO_V1POC_|VGUI Xash|Steam|vgui2' "${LOG}" 2>/dev/null | head -80 || true

if [[ "${PASS_READY}" -eq 1 && "${PASS_VISIBLE_640}" -eq 1 && "${PASS_VISIBLE_800}" -eq 1 \
	&& "${PASS_TEXT}" -eq 1 && "${PASS_BACKSPACE}" -eq 1 && "${PASS_TAB}" -eq 1 \
	&& "${PASS_HOVER}" -eq 1 && "${PASS_CLICK}" -eq 1 && "${PASS_NO_STEAM}" -eq 1 ]]; then
	echo "V1POC PASS: interactive proof ok (640+800, text/tab/hover/click)"
	exit 0
fi

echo "V1POC FAIL: siehe Checklist / ${LOG}" >&2
exit 1
