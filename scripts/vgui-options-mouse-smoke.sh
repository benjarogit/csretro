#!/usr/bin/env bash
# Options Mouse-Seite: Auto-Open + Marker-Check (kein Interim).
# Usage: ./scripts/build-menu.sh && ./scripts/vgui-options-mouse-smoke.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
# shellcheck source=headless-x11.sh
source "${ROOT}/scripts/headless-x11.sh"

RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run}"
case "${RUN}" in /*) ;; *) RUN="${ROOT}/${RUN}" ;; esac
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
LOG="${RUN}/engine.log"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
cd "${ROOT}"

fail() { echo "OPTIONS_MOUSE FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt — oder CSRETRO_FOREGROUND=1"
fi

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve" "${RUN}/cstrike/resource" \
	"${GAMEDATA}/cstrike/resource"
cp -a "${ROOT}/data/ui-overrides/cstrike/resource/OptionsSubMouse.res" \
	"${GAMEDATA}/cstrike/resource/OptionsSubMouse.res"
cp -a "${ROOT}/data/ui-overrides/cstrike/resource/OptionsSubMouse.res" \
	"${RUN}/cstrike/resource/OptionsSubMouse.res"
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
sensitivity 3
look_filter 0
m_rawinput 1
echo CSRETRO_OPTIONS_MOUSE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_OPTIONS_AUTO=1
unset CSRETRO_V1POC 2>/dev/null || true
export CSRETRO_RUN_DIR="${RUN}"

# ASan-Menü braucht Runtime im gamescope-Kind (LD_PRELOAD durchreichen).
if [[ "${MENU}" == *sanitize* ]] || nm -D "${MENU}" 2>/dev/null | rg -q '__asan_'; then
	ASAN_SO="${CSRETRO_ASAN_SO:-/usr/lib/clang/22/lib/linux/libclang_rt.asan-x86_64.so}"
	[[ -f "${ASAN_SO}" ]] || fail "ASan-Runtime fehlt: ${ASAN_SO}"
	export LD_PRELOAD="${ASAN_SO}${LD_PRELOAD:+:${LD_PRELOAD}}"
	export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=0}"
	export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=0}"
	echo "OPTIONS_MOUSE: ASan-Menü — LD_PRELOAD=${ASAN_SO}" >&2
fi

WIDTH=800
HEIGHT=600
export CSRETRO_GAMESCOPE_W="${WIDTH}"
export CSRETRO_GAMESCOPE_H="${HEIGHT}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-options-mouse.log"

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

trap 'stop_engine "${XASH_PID:-}"' EXIT

csretro_headless_x11_prepare || fail "gamescope"
rm -f "${LOG}"
: >"${CSRETRO_GAMESCOPE_LOG}"
killall -q xash3d 2>/dev/null || true
sleep 0.3

cd "${RUN}"
set +e
csretro_headless_x11_wrap ./xash3d \
	-game cstrike \
	-dll "${GAMEDLL}" \
	-clientlib "${CLIENT}" \
	-menu "${MENU}" \
	-windowed -width "${WIDTH}" -height "${HEIGHT}" \
	-dev 2 \
	-log \
	+maxplayers 2 \
	+sv_lan 1 \
	+exec autoexec.cfg \
	>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
XASH_PID=$!
CSRETRO_GAMESCOPE_PID="${XASH_PID}"
set -e
cd "${ROOT}"

csretro_headless_x11_wait_display || fail "headless-X11 nicht bereit"

if ! wait_log 'CSRETRO_OPTIONS_VISIBLE' 50; then
	tail -80 "${LOG}" 2>/dev/null || true
	tail -80 "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null || true
	fail "Timeout — CSRETRO_OPTIONS_VISIBLE fehlt"
fi

if rg -q 'CSRETRO_OPTIONS_INTERIM' "${LOG}" 2>/dev/null || rg -q 'CSRETRO_OPTIONS_INTERIM' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
	fail "Interim-Fallback trotz Mouse-Page"
fi

if rg -q 'ERROR: AddressSanitizer|ERROR: UndefinedBehaviorSanitizer|SEGV|Signal 11' "${LOG}" 2>/dev/null \
	|| rg -q 'ERROR: AddressSanitizer|ERROR: UndefinedBehaviorSanitizer|SEGV|Signal 11' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
	fail "Fatal Sanitizer/Crash im Log"
fi

# Vendor-SDK UBSan (alignment/mempool) ist bekannt und nicht Gate-kritisch; ASan ERROR zählt.
echo "OPTIONS_MOUSE PASS: CSRETRO_OPTIONS_VISIBLE, no interim, no fatal ASan"
