#!/usr/bin/env bash
# Scoreboard-Gate: Team → T → Class → Spawn → +showscores → mittige VGUI-Tafel.
# Eigene Familie: kein Team-Viewport, kein KEY_DEST_MENU. Orange HUD-Tafel aus.
# Usage: ./scripts/build-menu.sh && ./scripts/build-client.sh && ./scripts/vgui-score-gate.sh
# Optional: CSRETRO_FOREGROUND=1, CSRETRO_GATE_RES=800x600
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
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
cd "${ROOT}"

fail() { echo "SCORE_GATE FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -f "${CLIENT}" ]] || fail "Client fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
MENU="$(readlink -f "${MENU}")"
CLIENT="$(readlink -f "${CLIENT}")"
GAMEDLL="$(readlink -f "${GAMEDLL}")"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

if [[ "${CSRETRO_FOREGROUND:-}" == "" && -n "${DISPLAY:-}" ]]; then
	export CSRETRO_FOREGROUND=1
	echo "SCORE_GATE: CSRETRO_FOREGROUND=1 (nativ; Gamescope-Teardown getrennt)"
fi
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt (oder CSRETRO_FOREGROUND=1)"
fi

export CSRETRO_ENGINE_OUT="${ENG}" CSRETRO_CLIENT_SO="${CLIENT}" \
	CSRETRO_GAMEDLL_SO="${GAMEDLL}" CSRETRO_MENU_SO="${MENU}"
csretro_gate_isolate_begin "score" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOT_DIR="${ROOT}/build/score-shots"
mkdir -p "${SHOT_DIR}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"

cat > "${RUN}/cstrike/autoexec.cfg" <<EOF
developer 2
mp_auto_join_team 0
mp_limitteams 0
mp_autoteambalance 0
bot_quota 0
_vgui_menus 1
brightness ${CSRETRO_GATE_BRIGHTNESS:-0.0}
gamma ${CSRETRO_GATE_GAMMA:-2.5}
echo CSRETRO_SCORE_GATE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"
cat > "${RUN}/cstrike/listenserver.cfg" <<'EOF'
mp_auto_join_team 0
mp_limitteams 0
mp_autoteambalance 0
bot_quota 0
sv_lan 1
EOF

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_SCORE_GATE=1
export CSRETRO_GATE_GRACEFUL_QUIT=1
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE CSRETRO_MAINMENU_GATE \
	CSRETRO_CREATE_GATE CSRETRO_BROWSER_GATE CSRETRO_TEAM_GATE CSRETRO_CLASS_GATE \
	CSRETRO_BUY_GATE CSRETRO_RADIO_GATE CSRETRO_PAUSE_GATE CSRETRO_SPEC_GATE \
	2>/dev/null || true
export CSRETRO_RUN_DIR="${RUN}"

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

clear_stale_engines() {
	pgrep -x xash3d >/dev/null 2>&1 || return 0
	echo "WARN: xash3d aus einem früheren Lauf aktiv — wird beendet" >&2
	pkill -TERM -x xash3d 2>/dev/null || true
	sleep 2
	if pgrep -x xash3d >/dev/null 2>&1; then
		pkill -KILL -x xash3d 2>/dev/null || true
		sleep 1
	fi
	pgrep -x xash3d >/dev/null 2>&1 && fail "xash3d lässt sich nicht beenden"
	return 0
}

stop_display() {
	csretro_headless_x11_stop
	sleep 0.3
}

terminate_after_failure() {
	local pid="${1:-}"
	[[ -n "${pid}" ]] || { stop_display; return; }
	kill -TERM "${pid}" 2>/dev/null || true
	sleep 1
	kill -KILL "${pid}" 2>/dev/null || true
	wait "${pid}" 2>/dev/null || true
	stop_display
}

run_one() {
	local W="$1" H="$2"
	export CSRETRO_GAMESCOPE_W="${W}"
	export CSRETRO_GAMESCOPE_H="${H}"
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-score-${W}x${H}.log"

	csretro_headless_x11_prepare || fail "gamescope"
	rm -f "${LOG}"
	: >"${CSRETRO_GAMESCOPE_LOG}"
	clear_stale_engines

	cd "${RUN}"
	set +e
	csretro_headless_x11_wrap ./xash3d \
		-game cstrike \
		-dll "${GAMEDLL}" \
		-clientlib "${CLIENT}" \
		-menulib "${MENU}" \
		-windowed -width "${W}" -height "${H}" \
		-dev 2 -log \
		+maxplayers 10 \
		+sv_lan 1 \
		+map "${MAP}" \
		+exec autoexec.cfg \
		>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	local XASH_PID=$!
	CSRETRO_GAMESCOPE_PID="${XASH_PID}"
	set -e
	cd "${ROOT}"

	csretro_headless_x11_wait_display \
		|| { terminate_after_failure "${XASH_PID}"; fail "X11 ${W}x${H}"; }
	wait_log 'CSRETRO_SCORE_GATE_DONE' 180 \
		|| { terminate_after_failure "${XASH_PID}"; fail "Gate-Lauf unvollständig ${W}x${H}"; }

	local quit_ok=1
	csretro_gate_wait_quit "${XASH_PID}" 30 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || quit_ok=0
	stop_display

	local ALL="${RUN}/merged-${W}x${H}.log"
	cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null > "${ALL}" || true

	[[ "${quit_ok}" -eq 1 ]] || fail "Engine-Shutdown nach quit nicht sauber ${W}x${H}"

	rg -q 'CSRETRO_SCORE_GATE_FAIL' "${ALL}" && {
		rg 'CSRETRO_SCORE' "${ALL}" | head -40 >&2
		fail "Gate meldet Fehler ${W}x${H}"
	}

	rg -q 'CSRETRO_SCORE_VGUI open' "${ALL}" || fail "Scoreboard-VGUI nicht geöffnet ${W}x${H}"
	rg -q 'CSRETRO_SCORE_GATE_OPEN visible=1' "${ALL}" \
		|| fail "Scoreboard-Audit fehlt ${W}x${H}"
	rg -q 'CSRETRO_SCORE_GATE_OPEN .*players=[1-9]' "${ALL}" \
		|| fail "Scoreboard ohne Spielerzeile ${W}x${H}"
	rg -q 'CSRETRO_SCORE_GATE_OPEN .*raw=0' "${ALL}" \
		|| fail "Scoreboard-Titel roh ${W}x${H}"

	mkdir -p "${SHOT_DIR}"
	find "${RUN}/cstrike" -maxdepth 2 \( -name '*.tga' -o -name '*.bmp' -o -name '*.png' \) \
		-printf '%T@ %p\n' 2>/dev/null | sort -n | tail -1 | while read -r _ shot; do
		[[ -n "${shot}" ]] || continue
		cp -a "${shot}" "${SHOT_DIR}/score-${W}x${H}.${shot##*.}" 2>/dev/null || true
	done

	echo "SCORE_GATE PASS ${W}x${H}"
}

RES_LIST=("800x600")
if [[ -n "${CSRETRO_GATE_RES:-}" ]]; then
	RES_LIST=("${CSRETRO_GATE_RES}")
fi

for res in "${RES_LIST[@]}"; do
	run_one "${res%x*}" "${res#*x}"
done

echo "SCORE_GATE PASS all resolutions"
