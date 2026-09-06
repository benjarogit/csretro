#!/usr/bin/env bash
# Class-Select Gate: In-Game Class-Wahl als echte VGUI2-Controls.
# Startet de_dust (kein Auto-Join), Team → T, prüft Classmenu_TER.res,
# Localization (kein Cstrike_ roh), ESC, joinclass 1, danach CT-Menü + joinclass 1, quit.
# Usage: ./scripts/build-menu.sh && ./scripts/build-client.sh && ./scripts/vgui-classselect-gate.sh
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

fail() { echo "CLASSSELECT_GATE FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -f "${CLIENT}" ]] || fail "Client fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
MENU="$(readlink -f "${MENU}")"
CLIENT="$(readlink -f "${CLIENT}")"
GAMEDLL="$(readlink -f "${GAMEDLL}")"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

if [[ "${CSRETRO_FOREGROUND:-}" == "" && -n "${DISPLAY:-}" ]]; then
	export CSRETRO_FOREGROUND=1
	echo "CLASSSELECT_GATE: CSRETRO_FOREGROUND=1 (nativ; Gamescope-Teardown getrennt)"
fi
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt (oder CSRETRO_FOREGROUND=1)"
fi

export CSRETRO_ENGINE_OUT="${ENG}" CSRETRO_CLIENT_SO="${CLIENT}" \
	CSRETRO_GAMEDLL_SO="${GAMEDLL}" CSRETRO_MENU_SO="${MENU}"
csretro_gate_isolate_begin "classselect" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOT_DIR="${ROOT}/build/classselect-shots"
mkdir -p "${SHOT_DIR}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"

cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
mp_auto_join_team 0
mp_limitteams 0
mp_autoteambalance 0
bot_quota 0
_vgui_menus 1
echo CSRETRO_CLASS_GATE_CFG
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
export CSRETRO_CLASS_GATE=1
export CSRETRO_GATE_GRACEFUL_QUIT=1
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE CSRETRO_MAINMENU_GATE \
	CSRETRO_CREATE_GATE CSRETRO_BROWSER_GATE CSRETRO_TEAM_GATE CSRETRO_BUY_GATE \
	CSRETRO_RADIO_GATE CSRETRO_PAUSE_GATE CSRETRO_SPEC_GATE CSRETRO_SCORE_GATE 2>/dev/null || true
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
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-classselect-${W}x${H}.log"

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
	wait_log 'CSRETRO_CLASS_GATE_DONE' 120 \
		|| { terminate_after_failure "${XASH_PID}"; fail "Gate-Lauf unvollständig ${W}x${H}"; }

	local quit_ok=1
	csretro_gate_wait_quit "${XASH_PID}" 30 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || quit_ok=0
	stop_display

	local ALL="${RUN}/merged-${W}x${H}.log"
	cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null > "${ALL}" || true

	[[ "${quit_ok}" -eq 1 ]] || fail "Engine-Shutdown nach quit nicht sauber ${W}x${H}"

	rg -q 'CSRETRO_CLASS_GATE_FAIL' "${ALL}" && {
		rg 'CSRETRO_CLASS' "${ALL}" | head -30 >&2
		fail "Gate meldet Fehler ${W}x${H}"
	}

	rg -q 'CSRETRO_CLASS_VGUI open type=26' "${ALL}" || fail "TER-Class-VGUI nicht geöffnet ${W}x${H}"
	rg -q 'CSRETRO_CLASS_VGUI open type=26 .*lineup=4 static=0' "${ALL}" \
		|| fail "TER-Studioaufstellung fehlt ${W}x${H}"
	rg -q 'CSRETRO_CLASS_GATE_OPEN .*visible=1' "${ALL}" || fail "TER-Gate-Audit fehlt ${W}x${H}"
	rg -q 'CSRETRO_CLASS_GATE_OPEN .*title=1 terror=1 leet=1 arctic=1 guerilla=1 auto=1 cancel=1' "${ALL}" \
		|| fail "Localization der TER-Labels fehlt ${W}x${H}"
	rg -q 'CSRETRO_CLASS_GATE_OPEN .*classinfo=1' "${ALL}" \
		|| fail "Class-Info zeigt Roh-Token oder Gate-Audit fehlt ${W}x${H}"
	rg -q '#Cstrike_Class_Info' "${ALL}" && fail "Roh-Token #Cstrike_Class_Info im Log ${W}x${H}"
	rg -q 'CSRETRO_CLASS_GATE_OPEN .*militia=0' "${ALL}" \
		|| fail "Militia auf CS-1.6 ${MAP} darf nicht sichtbar sein ${W}x${H}"
	rg -q 'CSRETRO_CLASS_GATE_ESC visible=0' "${ALL}" \
		|| fail "ESC schließt die Class-Wahl nicht ${W}x${H}"
	rg -q 'CSRETRO_CLASS_CMD joinclass 1' "${ALL}" \
		|| fail "Taste 1 sendet joinclass 1 nicht ${W}x${H}"
	rg -q 'CSRETRO_CLASS_VGUI open type=27' "${ALL}" || fail "CT-Class-VGUI nicht geöffnet ${W}x${H}"
	rg -q 'CSRETRO_CLASS_GATE_CT .*title=1 urban=1 gsg9=1 sas=1 gign=1 auto=1' "${ALL}" \
		|| fail "Localization der CT-Labels fehlt ${W}x${H}"
	rg -q 'CSRETRO_CLASS_GATE_CT .*spetsnaz=0 preview=1' "${ALL}" \
		|| fail "Spetsnaz auf CS-1.6 ${MAP} darf nicht sichtbar sein ${W}x${H}"
	for model in terror leet arctic guerilla; do
		rg -q "CSRETRO_TEAM_MODEL path=models/player/${model}/${model}\\.mdl weapon=models/p_ak47\\.mdl player=1 weapon_index=[1-9][0-9]* .*scene=4" "${ALL}" \
			|| fail "TER-Klassenmodell ${model} fehlt ${W}x${H}"
	done
	for model in urban gsg9 sas gign; do
		rg -q "CSRETRO_TEAM_MODEL path=models/player/${model}/${model}\\.mdl weapon=models/p_m4a1\\.mdl player=1 weapon_index=[1-9][0-9]* .*scene=4" "${ALL}" \
			|| fail "CT-Klassenmodell ${model} fehlt ${W}x${H}"
	done
	rg -q 'CSRETRO_LOC_MISSING' "${ALL}" && {
		rg 'CSRETRO_LOC_MISSING' "${ALL}" | head >&2
		fail "fehlende Localization-Tokens ${W}x${H}"
	}
	rg -q 'CSRETRO_LOC_cstrike OK' "${ALL}" \
		|| fail "cstrike-Localization nicht geladen ${W}x${H}"

	mkdir -p "${SHOT_DIR}"
	find "${RUN}/cstrike" -maxdepth 2 \( -name '*.tga' -o -name '*.bmp' -o -name '*.png' \) \
		-printf '%T@ %p\n' 2>/dev/null | sort -n | tail -1 | while read -r _ shot; do
		[[ -n "${shot}" ]] || continue
		cp -a "${shot}" "${SHOT_DIR}/classselect-${W}x${H}.${shot##*.}" 2>/dev/null || true
	done

	echo "CLASSSELECT_GATE PASS ${W}x${H}"
}

RES_LIST=("800x600")
if [[ -n "${CSRETRO_GATE_RES:-}" ]]; then
	RES_LIST=("${CSRETRO_GATE_RES}")
fi

for res in "${RES_LIST[@]}"; do
	run_one "${res%x*}" "${res#*x}"
done

echo "CLASSSELECT_GATE PASS all resolutions"
