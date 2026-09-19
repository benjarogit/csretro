#!/usr/bin/env bash
# Genuine documentation captures; separate runtime, no user configuration edits.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-isolate.sh"
for dep in gamescope xdotool import; do
    command -v "${dep}" >/dev/null || { echo "Missing: ${dep}" >&2; exit 1; }
done
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" de_dust)"
for binary in "${ENG}/game_launch/xash3d" "${CLIENT}" "${MENU}" "${GAMEDLL}"; do
    [[ -f "${binary}" ]] || { echo "Missing: ${binary}" >&2; exit 1; }
done
# The existing helper clears its destination; give it a freshly allocated directory only.
CSRETRO_GATE_ISOLATE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/csretro-doc-capture.XXXXXX")"
export CSRETRO_GATE_ISOLATE_DIR
export CSRETRO_ENGINE_OUT="${ENG}" CSRETRO_CLIENT_SO="${CLIENT}"
export CSRETRO_MENU_SO="${MENU}" CSRETRO_GAMEDLL_SO="${GAMEDLL}"
csretro_gate_isolate_begin documentation
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
SHOTS="${RUN}/screenshots"
mkdir -p "${SHOTS}"
cp "${ROOT}/scripts/docs-capture.cfg" "${RUN}/cstrike/autoexec.cfg"
cp "${ROOT}/scripts/docs-capture.cfg" "${RUN}/cstrike/config.cfg"
export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}" XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_FOREGROUND=0 CSRETRO_GAMESCOPE_W=1280 CSRETRO_GAMESCOPE_H=720
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope.log"
stop_capture() {
    if [[ -n "${CAPTURE_PID:-}" ]]; then
        kill -TERM "${CAPTURE_PID}" 2>/dev/null || true
        wait "${CAPTURE_PID}" 2>/dev/null || true
    fi
}
trap stop_capture EXIT
csretro_headless_x11_prepare
cd "${RUN}"
csretro_headless_x11_wrap "${ENG}/game_launch/xash3d" -game cstrike \
    -clientlib "${CLIENT}" -dll "${GAMEDLL}" -menulib "${MENU}" \
    -windowed -width 1280 -height 720 +exec autoexec.cfg \
    >>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
CAPTURE_PID=$!
csretro_headless_x11_wait_display || { echo "Capture display unavailable" >&2; exit 1; }
WID=""
for _ in $(seq 1 100); do
    WID="$(xdotool search --onlyvisible --name 'CS Retro' 2>/dev/null | head -n1 || true)"
    [[ -n "${WID}" ]] && break
    sleep 0.1
done
[[ -n "${WID}" ]] || { echo "Game window missing" >&2; exit 1; }
capture() { import -window "${WID}" "${SHOTS}/$1.png"; }
press() { xdotool key --window "${WID}" "$1" >/dev/null 2>&1; }
sleep 3
capture main-menu
press F5
sleep 6
capture team-selection
press 1
sleep 2
capture class-selection
press 2
sleep 3
capture gameplay
press F10
sleep 2
capture buy-menu
press Escape
press F9
sleep 2
capture team-live
echo "Captures: ${SHOTS}"
echo "Base revision: $(git -C "${ROOT}" rev-parse HEAD)"
echo "Binary hashes (capture may use a local development build):"
sha256sum "${CLIENT}" "${MENU}" "${GAMEDLL}"
echo "Inspect all images before publication. Runtime retained at ${RUN}."
