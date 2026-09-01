#!/usr/bin/env bash
# CS Retro manuell starten (Fenster bleibt offen).
# Usage:
#   ./scripts/play.sh
#   CSRETRO_V1POC=1 ./scripts/play.sh          # V1-PoC-Dialog
#   ./scripts/play.sh -width 1920 -height 1080 # Extra-Args an xash3d
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
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${CSRETRO_SMOKE_MAP:-de_dust}")" || {
	echo "play: Game-Data fehlt — python3 ./scripts/bootstrap-gamedata.py" >&2
	exit 1
}

[[ -x "${ENG}/game_launch/xash3d" ]] || { echo "play: Engine fehlt — ./scripts/build-engine.sh" >&2; exit 1; }
[[ -f "${CLIENT}" ]] || { echo "play: Client fehlt — ./scripts/build-client.sh" >&2; exit 1; }
[[ -f "${GAMEDLL}" ]] || { echo "play: GameDLL fehlt — ./scripts/build-gamedll.sh" >&2; exit 1; }
[[ -f "${MENU}" ]] || { echo "play: Menü fehlt — ./scripts/build-menu.sh" >&2; exit 1; }

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve"
cp -a "${GAMEDLL}" "${RUN}/cstrike/dlls/cs_amd64.so"
cp -a "${CLIENT}" "${RUN}/cstrike/cl_dlls/client_amd64.so"
cp -a "${MENU}" "${RUN}/menu_amd64.so"
ln -sfn "${MENU}" "${RUN}/libmenu.so"
ln -sfn "${ENG}/engine/libxash.so" "${RUN}/libxash.so"
ln -sfn "${ENG}/ref/gl/libref_gl.so" "${RUN}/libref_gl.so"
ln -sfn "${ENG}/filesystem/filesystem_stdio.so" "${RUN}/filesystem_stdio.so"
ln -sfn "${ENG}/game_launch/xash3d" "${RUN}/xash3d"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"

cd "${RUN}"
exec ./xash3d -game cstrike \
	-dll cstrike/dlls/cs_amd64.so \
	-clientlib cstrike/cl_dlls/client_amd64.so \
	-menu menu_amd64.so \
	-windowed -width "${WIDTH}" -height "${HEIGHT}" \
	"$@"
