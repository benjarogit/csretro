#!/usr/bin/env bash
# Server-Browser Gate: LAN-Dialog als echte VGUI2-Controls.
# Prüft: Dialog öffnet, localservers geht, Filter (fremd/gs/Duplikat), Auswahl,
# Rescan leert die Liste, ESC schließt. Kein Connect.
# Usage: ./scripts/build-menu.sh && ./scripts/vgui-serverbrowser-gate.sh
# Optional: CSRETRO_FOREGROUND=1, CSRETRO_GATE_RES=800x600
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-crash.sh"

RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run-gate/serverbrowser}"
case "${RUN}" in /*) ;; *) RUN="${ROOT}/${RUN}" ;; esac
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
LOG="${RUN}/engine.log"
SHOT_DIR="${ROOT}/build/serverbrowser-shots"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
cd "${ROOT}"

fail() { echo "BROWSER_GATE FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
MENU="$(readlink -f "${MENU}")"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
# Nativ bevorzugen, wenn ein Display da ist: unter gamescope ist der beobachtete
# Exit-Code der des Wrappers, nicht der der Engine — gleiche Konvention wie
# scripts/vgui-creategame-gate.sh / vgui-graceful-shutdown-gate.sh.
if [[ "${CSRETRO_FOREGROUND:-}" == "" && -n "${DISPLAY:-}" ]]; then
	export CSRETRO_FOREGROUND=1
	echo "BROWSER_GATE: CSRETRO_FOREGROUND=1 (nativ; Gamescope-Teardown getrennt)"
fi
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt (oder CSRETRO_FOREGROUND=1)"
fi

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve" "${SHOT_DIR}"
cp -a "${GAMEDLL}" "${RUN}/cstrike/dlls/cs_amd64.so"
cp -a "${CLIENT}" "${RUN}/cstrike/cl_dlls/client_amd64.so"
cp -a "${MENU}" "${RUN}/menu_amd64.so"
ln -sfn "${MENU}" "${RUN}/libmenu.so"
ln -sfn "${ENG}/engine/libxash.so" "${RUN}/libxash.so"
ln -sfn "${ENG}/ref/gl/libref_gl.so" "${RUN}/libref_gl.so"
ln -sfn "${ENG}/filesystem/filesystem_stdio.so" "${RUN}/filesystem_stdio.so"
ln -sfn "${ENG}/game_launch/xash3d" "${RUN}/xash3d"

cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
echo CSRETRO_BROWSER_GATE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_BROWSER_GATE=1
export CSRETRO_GATE_GRACEFUL_QUIT=1
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE CSRETRO_MAINMENU_GATE CSRETRO_CREATE_GATE 2>/dev/null || true
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

# Ein übriggebliebener Prozess ist ein Leck eines früheren Laufs und verfälscht die
# Messung. Deshalb melden statt still aufräumen — und das Aufräumen auch nachprüfen.
clear_stale_engines() {
	pgrep -x xash3d >/dev/null 2>&1 || return 0

	echo "WARN: xash3d aus einem früheren Lauf aktiv — wird beendet" >&2
	pkill -TERM -x xash3d 2>/dev/null || true
	sleep 2
	if pgrep -x xash3d >/dev/null 2>&1; then
		echo "WARN: reagiert nicht auf SIGTERM (hängende Engine) — SIGKILL" >&2
		pkill -KILL -x xash3d 2>/dev/null || true
		sleep 1
	fi
	pgrep -x xash3d >/dev/null 2>&1 && fail "xash3d lässt sich nicht beenden — Lauf wäre nicht aussagekräftig"
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
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-serverbrowser-${W}x${H}.log"

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
		+exec autoexec.cfg \
		>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	local XASH_PID=$!
	CSRETRO_GAMESCOPE_PID="${XASH_PID}"
	set -e
	cd "${ROOT}"

	csretro_headless_x11_wait_display \
		|| { terminate_after_failure "${XASH_PID}"; fail "X11 ${W}x${H}"; }
	wait_log 'CSRETRO_BROWSER_GATE_DONE' 60 \
		|| { terminate_after_failure "${XASH_PID}"; fail "Gate-Lauf unvollständig ${W}x${H}"; }

	local quit_ok=1
	csretro_gate_wait_quit "${XASH_PID}" 30 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || quit_ok=0
	stop_display

	local ALL="${RUN}/merged-${W}x${H}.log"
	cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null > "${ALL}" || true

	[[ "${quit_ok}" -eq 1 ]] || fail "Engine-Shutdown nach quit nicht sauber ${W}x${H}"

	rg -q 'CSRETRO_BROWSER_GATE_FAIL' "${ALL}" && {
		rg 'CSRETRO_BROWSER' "${ALL}" | head -20 >&2
		fail "Gate meldet Fehler ${W}x${H}"
	}
	rg -q 'CSRETRO_BROWSER_UNAVAILABLE' "${ALL}" && fail "Dialog nicht verfügbar ${W}x${H}"

	rg -q 'CSRETRO_BROWSER_GATE_OPEN visible=1' "${ALL}" || {
		rg 'CSRETRO_BROWSER' "${ALL}" | head -20 >&2
		fail "Dialog nicht sichtbar ${W}x${H}"
	}

	# Engine hat „localservers“ wirklich ausgeführt — nicht nur das Menü-Echo.
	rg -q 'Scanning for servers on the local network' "${ALL}" || {
		rg 'localservers|Scanning for servers' "${ALL}" >&2
		fail "Engine hat localservers nicht ausgeführt ${W}x${H}"
	}

	rg -q 'CSRETRO_BROWSER_GATE_VALID rows=2' "${ALL}" || {
		rg 'CSRETRO_BROWSER_GATE_VALID|CSRETRO_BROWSER_ADD|CSRETRO_BROWSER_REJECT' "${ALL}" >&2
		fail "Zwei gültige Server fehlen ${W}x${H}"
	}

	# Fremd-Mod, GoldSrc-gs und Duplikat dürfen die Zeilenzahl nicht erhöhen.
	rg -q 'CSRETRO_BROWSER_GATE_FILTER rows=2' "${ALL}" || {
		rg 'CSRETRO_BROWSER_GATE_FILTER|CSRETRO_BROWSER_REJECT|CSRETRO_BROWSER_ADD' "${ALL}" >&2
		fail "Filter lässt Fremd-/gs-/Duplikat durch ${W}x${H}"
	}

	rg -q 'CSRETRO_BROWSER_GATE_SELECT selected=1 connect=1' "${ALL}" || {
		rg 'CSRETRO_BROWSER_GATE_SELECT' "${ALL}" >&2
		fail "Auswahl aktiviert Connect nicht ${W}x${H}"
	}

	rg -q 'CSRETRO_BROWSER_GATE_RESCAN rows=0' "${ALL}" || {
		rg 'CSRETRO_BROWSER_GATE_RESCAN' "${ALL}" >&2
		fail "Rescan leert die Liste nicht ${W}x${H}"
	}

	rg -q 'CSRETRO_BROWSER_GATE_ESC visible=0' "${ALL}" || {
		rg 'CSRETRO_BROWSER_GATE_ESC' "${ALL}" >&2
		fail "ESC schließt den Dialog nicht ${W}x${H}"
	}

	rg -q 'CSRETRO_LOC_csretro_gameui OK' "${ALL}" \
		|| fail "csretro_gameui-Localization nicht geladen ${W}x${H}"
	rg -q 'CSRETRO_LOC_MISSING' "${ALL}" && {
		rg 'CSRETRO_LOC_MISSING' "${ALL}" | head >&2
		fail "fehlende Localization-Tokens ${W}x${H}"
	}

	echo "BROWSER_GATE PASS ${W}x${H}"
}

RES_LIST=("800x600" "1024x768")
if [[ -n "${CSRETRO_GATE_RES:-}" ]]; then
	RES_LIST=("${CSRETRO_GATE_RES}")
fi

for res in "${RES_LIST[@]}"; do
	run_one "${res%x*}" "${res#*x}"
done

echo "BROWSER_GATE PASS all resolutions"
