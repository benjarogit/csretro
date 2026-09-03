#!/usr/bin/env bash
# VGUI graceful-shutdown Gate: Options + Combo, then quit, wait exit==0.
# Usage:
#   ./scripts/build-menu.sh && ./scripts/vgui-graceful-shutdown-gate.sh
# Sanitize:
#   ./scripts/build-menu.sh --sanitize && \
#     CSRETRO_MENU_SO=build/menu-sanitize/menu/menu_amd64.so ./scripts/vgui-graceful-shutdown-gate.sh
# Native (no Gamescope WSI):
#   CSRETRO_FOREGROUND=1 ./scripts/vgui-graceful-shutdown-gate.sh
# Loops: CSRETRO_SHUTDOWN_LOOPS=3 (default)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-crash.sh"

RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run-gate/shutdown}"
case "${RUN}" in /*) ;; *) RUN="${ROOT}/${RUN}" ;; esac
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
LOG="${RUN}/engine.log"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
W="${CSRETRO_WIDTH:-800}"
H="${CSRETRO_HEIGHT:-600}"
LOOPS="${CSRETRO_SHUTDOWN_LOOPS:-3}"
cd "${ROOT}"

fail() { echo "VGUI_GRACEFUL_SHUTDOWN FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
MENU="$(readlink -f "${MENU}")"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

# Prefer native when DISPLAY is available — Gamescope WSI is a separate issue.
if [[ "${CSRETRO_FOREGROUND:-}" == "" && -n "${DISPLAY:-}" ]]; then
	export CSRETRO_FOREGROUND=1
	echo "VGUI_GRACEFUL_SHUTDOWN: defaulting CSRETRO_FOREGROUND=1 (native; Gamescope WSI separated)"
fi
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt (oder CSRETRO_FOREGROUND=1)"
fi

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve" "${RUN}/cstrike/resource" \
	"${GAMEDATA}/cstrike/resource" "${GAMEDATA}/valve/resource" "${RUN}/valve/resource"
for f in OptionsSubMouse.res OptionsSubAudio.res OptionsSubVideo.res csretro_gameui_english.txt; do
	cp -a "${ROOT}/data/ui-overrides/cstrike/resource/${f}" "${GAMEDATA}/cstrike/resource/${f}"
	cp -a "${ROOT}/data/ui-overrides/cstrike/resource/${f}" "${RUN}/cstrike/resource/${f}"
done
cp -a "${ROOT}/data/ui-overrides/cstrike/resource/OptionsSubVideo.res" \
	"${GAMEDATA}/valve/resource/OptionsSubVideo.res"
cp -a "${ROOT}/data/ui-overrides/cstrike/resource/OptionsSubVideo.res" \
	"${RUN}/valve/resource/OptionsSubVideo.res"
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
fullscreen 0
echo CSRETRO_VGUI_SHUTDOWN_GATE_CFG
EOF

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_OPTIONS_VIDEO_GATE=1
export CSRETRO_GATE_GRACEFUL_QUIT=1
export CSRETRO_POOL_AUDIT="${CSRETRO_POOL_AUDIT:-1}"
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE CSRETRO_OPTIONS_AUDIO_GATE \
	CSRETRO_OPTIONS_VIDEO_MODE_SAFETY 2>/dev/null || true
export CSRETRO_RUN_DIR="${RUN}"

if [[ "${MENU}" == *sanitize* ]] || nm -D "${MENU}" 2>/dev/null | rg -q '__asan_'; then
	ASAN_SO="${CSRETRO_ASAN_SO:-/usr/lib/clang/22/lib/linux/libclang_rt.asan-x86_64.so}"
	[[ -f "${ASAN_SO}" ]] || fail "ASan-Runtime fehlt"
	export LD_PRELOAD="${ASAN_SO}${LD_PRELOAD:+:${LD_PRELOAD}}"
	export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=0:abort_on_error=1}"
	export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=0}"
fi

wait_log() {
	local pat="$1" secs="$2" i
	for i in $(seq 1 "${secs}"); do
		if rg -q "${pat}" "${LOG}" 2>/dev/null || rg -q "${pat}" "${CSRETRO_GAMESCOPE_LOG:-/dev/null}" 2>/dev/null; then
			return 0
		fi
		sleep 1
	done
	return 1
}

export CSRETRO_GAMESCOPE_W="${W}"
export CSRETRO_GAMESCOPE_H="${H}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-vgui-shutdown-gate.log"

run_one() {
	local n="$1"
	echo "==== VGUI graceful shutdown loop ${n}/${LOOPS} ===="
	if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
		csretro_headless_x11_prepare || fail "gamescope"
	else
		CSRETRO_HEADLESS=0
		export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
		export DISPLAY="${DISPLAY:-:0}"
		unset ENABLE_GAMESCOPE_WSI 2>/dev/null || true
		export DISABLE_GAMESCOPE_WSI=1
	fi
	rm -f "${LOG}"
	: >"${CSRETRO_GAMESCOPE_LOG}"
	killall -q xash3d gamescope zenity 2>/dev/null || true
	sleep 0.2

	cd "${RUN}"
	set +e
	if [[ "${CSRETRO_FOREGROUND:-0}" == 1 ]]; then
		./xash3d \
			-game cstrike \
			-dll "${GAMEDLL}" \
			-clientlib "${CLIENT}" \
			-menulib "${MENU}" \
			-windowed -width "${W}" -height "${H}" \
			-dev 2 -log \
			+maxplayers 2 +sv_lan 1 +exec autoexec.cfg \
			>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	else
		csretro_headless_x11_wrap ./xash3d \
			-game cstrike \
			-dll "${GAMEDLL}" \
			-clientlib "${CLIENT}" \
			-menulib "${MENU}" \
			-windowed -width "${W}" -height "${H}" \
			-dev 2 -log \
			+maxplayers 2 +sv_lan 1 +exec autoexec.cfg \
			>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	fi
	local XASH_PID=$!
	CSRETRO_GAMESCOPE_PID="${XASH_PID}"
	set -e
	cd "${ROOT}"

	if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
		csretro_headless_x11_wait_display || {
			kill -KILL "${XASH_PID}" 2>/dev/null || true
			fail "X11 loop ${n}"
		}
	fi

	wait_log 'CSRETRO_VIDEO_GATE_DONE' 90 || {
		rg -n 'CSRETRO_|signal|Abort|asan|runtime error' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | tail -60 >&2 || true
		kill -KILL "${XASH_PID}" 2>/dev/null || true
		fail "GATE_DONE loop ${n}"
	}

	if rg -q 'CSRETRO_VIDEO_GATE_FAIL|CSRETRO_COMBO_GATE_FAIL' "${LOG}" 2>/dev/null \
		|| rg -q 'CSRETRO_VIDEO_GATE_FAIL|CSRETRO_COMBO_GATE_FAIL' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
		fail "Gate assertions loop ${n}"
	fi

	# quit already issued by menu when CSRETRO_GATE_GRACEFUL_QUIT=1; wait for clean exit.
	local i ec=0
	for i in $(seq 1 30); do
		if ! kill -0 "${XASH_PID}" 2>/dev/null; then
			set +e
			wait "${XASH_PID}"
			ec=$?
			set -e
			break
		fi
		sleep 1
	done
	if kill -0 "${XASH_PID}" 2>/dev/null; then
		echo "still alive after quit — TERM" >&2
		kill -TERM "${XASH_PID}" 2>/dev/null || true
		sleep 2
		if kill -0 "${XASH_PID}" 2>/dev/null; then
			kill -KILL "${XASH_PID}" 2>/dev/null || true
			wait "${XASH_PID}" 2>/dev/null || true
			fail "quit timeout loop ${n}"
		fi
		set +e
		wait "${XASH_PID}"
		ec=$?
		set -e
	fi

	csretro_headless_x11_stop

	if csretro_gate_logs_indicate_crash "${LOG}" "${CSRETRO_GAMESCOPE_LOG}"; then
		rg -n "$(csretro_gate_crash_pattern)" "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | head -50 >&2 || true
		fail "crash signature loop ${n} exit=${ec}"
	fi
	if [[ "${ec}" -ne 0 ]]; then
		fail "unclean exit ${ec} loop ${n}"
	fi
	echo "VGUI_GRACEFUL_SHUTDOWN PASS loop ${n} exit=0"
}

for n in $(seq 1 "${LOOPS}"); do
	run_one "${n}"
done

echo "VGUI_GRACEFUL_SHUTDOWN PASS all ${LOOPS} loops"
