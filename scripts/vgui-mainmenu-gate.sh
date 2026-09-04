#!/usr/bin/env bash
# Main-Menu Gate: klassisches Hauptmenü als echte VGUI2-Controls.
# Prüft: GameMenu.res-Items geladen, Localization aufgelöst, Layout unten links,
# Items innerhalb des Screens, OnlyInGame-Einträge im Hauptmenü unsichtbar.
# Usage: ./scripts/build-menu.sh && ./scripts/vgui-mainmenu-gate.sh
# Optional: CSRETRO_FOREGROUND=1, CSRETRO_GATE_RES=640x480 (default mehrere)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"

RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run-gate/mainmenu}"
case "${RUN}" in /*) ;; *) RUN="${ROOT}/${RUN}" ;; esac
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
LOG="${RUN}/engine.log"
SHOT_DIR="${ROOT}/build/mainmenu-shots"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
cd "${ROOT}"

fail() { echo "MAINMENU_GATE FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
MENU="$(readlink -f "${MENU}")"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt"
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
echo CSRETRO_MAINMENU_GATE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_MAINMENU_GATE=1
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE 2>/dev/null || true
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

stop_engine() {
	local pid="${1:-}"
	[[ -n "${pid}" ]] && kill "${pid}" 2>/dev/null || true
	csretro_headless_x11_stop
	killall -q xash3d 2>/dev/null || true
	sleep 0.3
}

run_one() {
	local W="$1" H="$2"
	export CSRETRO_GAMESCOPE_W="${W}"
	export CSRETRO_GAMESCOPE_H="${H}"
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-mainmenu-${W}x${H}.log"

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
		+exec autoexec.cfg \
		>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	local XASH_PID=$!
	CSRETRO_GAMESCOPE_PID="${XASH_PID}"
	set -e
	cd "${ROOT}"

	csretro_headless_x11_wait_display || { stop_engine "${XASH_PID}"; fail "X11 ${W}x${H}"; }
	wait_log 'CSRETRO_MAINMENU_LAYOUT' 40 || { stop_engine "${XASH_PID}"; fail "kein Layout ${W}x${H}"; }
	sleep 1.0

	local shot="${SHOT_DIR}/csretro-mainmenu-${W}x${H}.png"
	wait_log 'CSRETRO_MAINMENU_SHOT_TAKEN' 20 || true
	sleep 1.5
	wait_log 'CSRETRO_MAINMENU_GATE_DONE' 30 || { stop_engine "${XASH_PID}"; fail "Survive-Lauf unvollständig ${W}x${H}"; }
	local newest
	newest="$(find "${RUN}/cstrike/scrshots" -maxdepth 2 \( -name '*.png' -o -name '*.bmp' \) -size +20k \
		-printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2- || true)"
	if [[ -n "${newest}" && -f "${newest}" ]]; then
		cp -a "${newest}" "${shot}"
	else
		echo "WARN: kein Screenshot ${W}x${H}" >&2
	fi

	local ALL="${RUN}/merged-${W}x${H}.log"
	cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null > "${ALL}" || true
	stop_engine "${XASH_PID}"

	rg -q 'CSRETRO_MAINMENU_NO_ITEMS' "${ALL}" && fail "GameMenu.res nicht geladen ${W}x${H}"

	# Kein unaufgelöstes Localization-Token: weder "#Token" noch das nackte Token.
	if rg -q 'CSRETRO_MAINMENU_ITEM .*text="(#|GameUI_|Valve_)' "${ALL}"; then
		rg 'CSRETRO_MAINMENU_ITEM' "${ALL}" | head -20 >&2
		fail "unaufgelöste Localization ${W}x${H}"
	fi

	# Hauptmenü ohne Level: OnlyInGame-Einträge müssen unsichtbar sein.
	if rg -q 'CSRETRO_MAINMENU_ITEM cmd=(ResumeGame|Disconnect|OpenPlayerListDialog) .*visible=1' "${ALL}"; then
		rg 'CSRETRO_MAINMENU_ITEM' "${ALL}" | head -20 >&2
		fail "OnlyInGame-Eintrag im Hauptmenü sichtbar ${W}x${H}"
	fi

	# Mindestens die vier Out-of-Game-Einträge.
	local visible
	visible="$(rg -o 'CSRETRO_MAINMENU_VISIBLE \d+' "${ALL}" | tail -1 | rg -o '\d+' || echo 0)"
	[[ "${visible}" -ge 4 ]] || fail "nur ${visible} sichtbare Einträge ${W}x${H}"

	# Regression: Hauptmenü überlebt Öffnen, Klick und Schließen der Options.
	rg -q 'CSRETRO_MAINMENU_SURVIVE_OPEN options=1 menu=1' "${ALL}" || {
		rg 'CSRETRO_MAINMENU_SURVIVE' "${ALL}" | head >&2
		fail "Klick auf Options: Dialog zu oder Hauptmenü weg ${W}x${H}"
	}
	rg -q 'CSRETRO_MAINMENU_SURVIVE_CLICK 1' "${ALL}" \
		|| fail "Hauptmenü nach Klick in den Dialog verschwunden ${W}x${H}"
	rg -q 'CSRETRO_MAINMENU_SURVIVE_ESC options=0 menu=1' "${ALL}" || {
		rg 'CSRETRO_MAINMENU_SURVIVE' "${ALL}" | head >&2
		fail "ESC: Options offen oder Hauptmenü weg ${W}x${H}"
	}
	rg -q 'CSRETRO_MAINMENU_SURVIVE panel=1 menu=1' "${ALL}" || {
		rg 'CSRETRO_MAINMENU_SURVIVE' "${ALL}" | head >&2
		fail "Hauptmenü nach Schließen der Options weg ${W}x${H}"
	}

	# Layout muss im Screen liegen und unten links sitzen.
	local layout mx my mw mh
	layout="$(rg -o 'CSRETRO_MAINMENU_LAYOUT screen=\d+x\d+ menu=\d+,\d+ \d+x\d+' "${ALL}" | tail -1)"
	[[ -n "${layout}" ]] || fail "Layout-Zeile fehlt ${W}x${H}"
	mx="$(echo "${layout}" | sed -E 's/.*menu=([0-9]+),([0-9]+) ([0-9]+)x([0-9]+)/\1/')"
	my="$(echo "${layout}" | sed -E 's/.*menu=([0-9]+),([0-9]+) ([0-9]+)x([0-9]+)/\2/')"
	mw="$(echo "${layout}" | sed -E 's/.*menu=([0-9]+),([0-9]+) ([0-9]+)x([0-9]+)/\3/')"
	mh="$(echo "${layout}" | sed -E 's/.*menu=([0-9]+),([0-9]+) ([0-9]+)x([0-9]+)/\4/')"
	[[ "${mw}" -gt 0 && "${mh}" -gt 0 ]] || fail "Menü ohne Größe ${W}x${H}"
	(( mx + mw <= W )) || fail "Menü ragt rechts heraus (${mx}+${mw} > ${W})"
	(( my + mh <= H )) || fail "Menü ragt unten heraus (${my}+${mh} > ${H})"
	# Unten links verankert: Unterkante in der unteren Bildhälfte, linke Kante links.
	(( my + mh > H / 2 )) || fail "Menü-Unterkante nicht in der unteren Hälfte (${my}+${mh}, H=${H})"
	(( mx < W / 2 )) || fail "Menü nicht linksbündig (x=${mx}, W=${W})"

	echo "MAINMENU_GATE PASS ${W}x${H} menu=${mx},${my} ${mw}x${mh} visible=${visible} shot=${shot}"
}

RES_LIST=("640x480" "800x600" "1024x768" "1366x768")
if [[ -n "${CSRETRO_GATE_RES:-}" ]]; then
	RES_LIST=("${CSRETRO_GATE_RES}")
fi

for res in "${RES_LIST[@]}"; do
	run_one "${res%x*}" "${res#*x}"
done

echo "MAINMENU_GATE PASS all resolutions"
