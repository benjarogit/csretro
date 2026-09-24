#!/usr/bin/env bash
# CS Retro manuell starten (Fenster bleibt offen).
# Usage:
#   ./scripts/play.sh
#   CSRETRO_KEYBOARD_CAPTURE_DEBUG=1 ./scripts/play.sh
#   CSRETRO_V1POC=1 ./scripts/play.sh
#   ./scripts/play.sh -width 1920 -height 1080
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run}"
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
WIDTH="${CSRETRO_WIDTH:-1280}"
HEIGHT="${CSRETRO_HEIGHT:-720}"

[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/play-sanitize-usercfg.sh"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${CSRETRO_SMOKE_MAP:-de_dust}")" || {
	echo "play: Game-Data fehlt — python3 ./scripts/bootstrap-gamedata.py" >&2
	exit 1
}

[[ -x "${ENG}/game_launch/xash3d" ]] || { echo "play: Engine fehlt — ./scripts/build-engine.sh" >&2; exit 1; }
[[ -f "${CLIENT}" ]] || { echo "play: Client fehlt — ./scripts/build-client.sh" >&2; exit 1; }
[[ -f "${GAMEDLL}" ]] || { echo "play: GameDLL fehlt — ./scripts/build-gamedll.sh" >&2; exit 1; }
[[ -f "${MENU}" ]] || { echo "play: Menü fehlt — ./scripts/build-menu.sh" >&2; exit 1; }

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve" "${RUN}/cfg" "${RUN}/cstrike/resource" "${RUN}/logs"
csretro_stage_valve_loc \
	"${ROOT}/data/ui-overrides/cstrike/resource/csretro_gameui_english.txt" \
	"${RUN}/cstrike/resource/csretro_gameui_english.txt"
csretro_stage_valve_loc \
	"${ROOT}/data/ui-overrides/cstrike/resource/csretro_gameui_german.txt" \
	"${RUN}/cstrike/resource/csretro_gameui_german.txt"
for play_txt in autobuy.txt rebuy.txt; do
	if [[ -f "${ROOT}/data/ui-overrides/cstrike/${play_txt}" && ! -e "${RUN}/cstrike/${play_txt}" ]]; then
		cp -a "${ROOT}/data/ui-overrides/cstrike/${play_txt}" "${RUN}/cstrike/${play_txt}"
	fi
done
if [[ -d "${ROOT}/data/ui-overrides/cstrike/resource/buy" ]]; then
	mkdir -p "${RUN}/cstrike/resource/buy"
	cp -a "${ROOT}/data/ui-overrides/cstrike/resource/buy/." "${RUN}/cstrike/resource/buy/"
fi
if [[ -d "${ROOT}/data/ui-overrides/cstrike/sound/announcer" ]]; then
	mkdir -p "${RUN}/cstrike/sound/announcer"
	cp -a "${ROOT}/data/ui-overrides/cstrike/sound/announcer/." "${RUN}/cstrike/sound/announcer/"
fi
PLAY_STAMP="$(date +%Y%m%d-%H%M%S)"
PLAY_LOG="${RUN}/logs/play-${PLAY_STAMP}.log"
PLAY_LOG_KEEP="${CSRETRO_PLAY_LOG_KEEP:-20}"
if ! [[ "${PLAY_LOG_KEEP}" =~ ^[0-9]+$ ]] || (( PLAY_LOG_KEEP < 1 )); then
	echo "play: CSRETRO_PLAY_LOG_KEEP must be a positive integer" >&2
	exit 1
fi
# Reserve one retained slot for the session log that tee creates below.
mapfile -t OLD_PLAY_LOGS < <(find "${RUN}/logs" -maxdepth 1 -type f -name 'play-*.log' -printf '%f\n' | sort -r | tail -n +${PLAY_LOG_KEEP})
for old_log in "${OLD_PLAY_LOGS[@]}"; do
	rm -f "${RUN}/logs/${old_log}"
done
ln -sfn "play-${PLAY_STAMP}.log" "${RUN}/logs/play-latest.log"
ln -sfn "logs/play-${PLAY_STAMP}.log" "${RUN}/play.log"
# Ab hier: Terminal und Session-Log gleichzeitig.
exec > >(tee -a "${PLAY_LOG}") 2>&1
echo "CSRETRO_PLAY_LOG session=${PLAY_LOG}"
echo "CSRETRO_PLAY_LOG latest=${RUN}/logs/play-latest.log"
echo "CSRETRO_PLAY_LOG engine=${RUN}/engine.log"
if [[ "${CSRETRO_PLAY_KEEP_FIXTURES:-0}" != 1 ]]; then
	csretro_play_sanitize_usercfg "${RUN}" "${GAMEDATA}"
fi
# Renderer/weapon A/B: infinite buy, no freeze, max money, buy anywhere.
# Create Game defaults: data/ui-overrides/cstrike/settings.scr (also staged here
# so Create Game / filesystem see play-test defaults without stale gamedata).
cp -a "${ROOT}/data/ui-overrides/cstrike/settings.scr" "${RUN}/cstrike/settings.scr"
cp -a "${ROOT}/scripts/play-test-rules.cfg" "${RUN}/cstrike/listenserver.cfg"
cp -a "${ROOT}/scripts/play-test-client.cfg" "${RUN}/cstrike/csretro_play_test.cfg"
if [[ ! -f "${RUN}/cstrike/userconfig.cfg" ]]; then
	printf '%s\n' \
		'// CS Retro userconfig — user bindings only.' \
		'// Automated 3C/gate aliases live in build/run-3c, not here.' \
		> "${RUN}/cstrike/userconfig.cfg"
fi
if ! rg -q 'csretro_play_test\.cfg' "${RUN}/cstrike/userconfig.cfg" 2>/dev/null; then
	printf '\n%s\n' 'exec csretro_play_test.cfg' >> "${RUN}/cstrike/userconfig.cfg"
fi
echo "CSRETRO_PLAY_TEST_RULES listenserver=maxmoney/buytime-1/freezetime0/buy_anywhere/round_infinite"
echo "CSRETRO_PLAY_TEST_CLIENT F5/F6/F7=renderer0/1/2 F8=shot F9/F10/F11=weapons F1=money F4=help"
csretro_play_print_exec_context "${RUN}" "${GAMEDATA}"
cp -a "${GAMEDLL}" "${RUN}/cstrike/dlls/cs_amd64.so"
cp -a "${CLIENT}" "${RUN}/cstrike/cl_dlls/client_amd64.so"
cp -a "${MENU}" "${RUN}/menu_amd64.so"
ln -sfn "${MENU}" "${RUN}/libmenu.so"
ln -sfn "${ENG}/engine/libxash.so" "${RUN}/libxash.so"
ln -sfn "${ENG}/ref/gl/libref_gl.so" "${RUN}/libref_gl.so"
ln -sfn "${ENG}/filesystem/filesystem_stdio.so" "${RUN}/filesystem_stdio.so"
ln -sfn "${ENG}/game_launch/xash3d" "${RUN}/xash3d"

MENU_ABS="$(readlink -f "${MENU}")"
RUN_MENU_ABS="$(readlink -f "${RUN}/menu_amd64.so")"
MENU_SHA="$(sha256sum "${MENU_ABS}" | awk '{print $1}')"
RUN_SHA="$(sha256sum "${RUN_MENU_ABS}" | awk '{print $1}')"
GIT_REV="$(git -C "${ROOT}" rev-parse --short=12 HEAD 2>/dev/null || echo nogit)"
MENU_MTIME="$(stat -c '%y' "${MENU_ABS}" 2>/dev/null || true)"

echo "CSRETRO_PLAY_PROVENANCE git=${GIT_REV}"
echo "CSRETRO_PLAY_PROVENANCE menu_src=${MENU_ABS}"
echo "CSRETRO_PLAY_PROVENANCE menu_run=${RUN_MENU_ABS}"
echo "CSRETRO_PLAY_PROVENANCE sha256_src=${MENU_SHA}"
echo "CSRETRO_PLAY_PROVENANCE sha256_run=${RUN_SHA}"
echo "CSRETRO_PLAY_PROVENANCE mtime=${MENU_MTIME}"
echo "CSRETRO_PLAY_PROVENANCE capture_debug=${CSRETRO_KEYBOARD_CAPTURE_DEBUG:-0}"
if [[ "${MENU_SHA}" != "${RUN_SHA}" ]]; then
	echo "play: FATAL menu SHA mismatch after copy" >&2
	exit 1
fi

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_MENU_SO="${MENU_ABS}"
export CSRETRO_MENU_SHA256="${MENU_SHA}"
export CSRETRO_MENU_GIT_REV="${GIT_REV}$(git -C "${ROOT}" diff --quiet || printf '+dirty')"
export CSRETRO_RUN_DIR="${RUN}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"

EXTRA_ARGS=(-log)
if [[ -n "${CSRETRO_KEYBOARD_CAPTURE_DEBUG:-}" && "${CSRETRO_KEYBOARD_CAPTURE_DEBUG}" != "0" ]]; then
	# Ensure Con_Printf + engine.log; capture also prints to stderr.
	EXTRA_ARGS+=(-dev 2)
	echo "CSRETRO_PLAY: capture debug on — watch stderr for [CSRETRO_KB] and ${RUN}/engine.log"
fi

cd "${RUN}"
# Absolute -menulib path so Xash cannot pick a stale relative/other menu.
set +e
./xash3d -game cstrike \
	-dll cstrike/dlls/cs_amd64.so \
	-clientlib cstrike/cl_dlls/client_amd64.so \
	-menulib "${MENU_ABS}" \
	-windowed -width "${WIDTH}" -height "${HEIGHT}" \
	"${EXTRA_ARGS[@]}" \
	"$@"
rc=$?
set -e
echo "CSRETRO_PLAY_EXIT ${rc}"
echo "CSRETRO_PLAY_LOG wrote ${PLAY_LOG}"
exit "${rc}"
