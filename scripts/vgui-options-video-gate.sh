#!/usr/bin/env bash
# Options Video Gate: Localization + Apply/OK/Cancel/Reset + Screenshots.
# Usage: ./scripts/build-menu.sh && ./scripts/vgui-options-video-gate.sh
# Optional: CSRETRO_FOREGROUND=1, CSRETRO_GATE_RES=640x480
# Note: automated gate does not toggle fullscreen/borderless (safety under gamescope).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"

RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run}"
case "${RUN}" in /*) ;; *) RUN="${ROOT}/${RUN}" ;; esac
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
LOG="${RUN}/engine.log"
SHOT_DIR="${ROOT}/build/options-video-shots"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
cd "${ROOT}"

fail() { echo "OPTIONS_VIDEO_GATE FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt"
fi

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve" "${RUN}/cstrike/resource" \
	"${GAMEDATA}/cstrike/resource" "${SHOT_DIR}" "${GAMEDATA}/valve/resource" "${RUN}/valve/resource"
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
brightness 0.5
gamma 2.2
gl_vsync 1
r_detailtextures 1
echo CSRETRO_OPTIONS_VIDEO_GATE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_OPTIONS_VIDEO_GATE=1
export CSRETRO_VGUI_METRICS_DUMP=1
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE CSRETRO_OPTIONS_AUDIO_GATE 2>/dev/null || true
export CSRETRO_RUN_DIR="${RUN}"

if [[ "${MENU}" == *sanitize* ]] || nm -D "${MENU}" 2>/dev/null | rg -q '__asan_'; then
	ASAN_SO="${CSRETRO_ASAN_SO:-/usr/lib/clang/22/lib/linux/libclang_rt.asan-x86_64.so}"
	[[ -f "${ASAN_SO}" ]] || fail "ASan-Runtime fehlt"
	export LD_PRELOAD="${ASAN_SO}${LD_PRELOAD:+:${LD_PRELOAD}}"
	export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=0}"
	export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=0}"
fi

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
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-options-video-gate-${W}x${H}.log"
	local shot="${SHOT_DIR}/csretro-options-video-${W}x${H}.png"

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
		-menu "${MENU}" \
		-windowed -width "${W}" -height "${H}" \
		-dev 2 -log \
		+maxplayers 2 +sv_lan 1 +exec autoexec.cfg \
		>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	local XASH_PID=$!
	CSRETRO_GAMESCOPE_PID="${XASH_PID}"
	set -e
	cd "${ROOT}"

	csretro_headless_x11_wait_display || { stop_engine "${XASH_PID}"; fail "X11 ${W}x${H}"; }

	wait_log 'CSRETRO_OPTIONS_VISIBLE' 40 || { stop_engine "${XASH_PID}"; fail "VISIBLE ${W}x${H}"; }
	wait_log 'CSRETRO_VIDEO_GATE_DONE' 60 || { stop_engine "${XASH_PID}"; fail "GATE_DONE ${W}x${H}"; }
	wait_log 'CSRETRO_VIDEO_GATE_SHOT_TAKEN|CSRETRO_VIDEO_GATE_SHOT_READY' 30 || true
	sleep 1.5

	if rg -q 'CSRETRO_VIDEO_GATE_FAIL' "${LOG}" 2>/dev/null || rg -q 'CSRETRO_VIDEO_GATE_FAIL' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
		rg 'CSRETRO_VIDEO_GATE_FAIL|CSRETRO_LOC_' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | head -40 >&2 || true
		stop_engine "${XASH_PID}"
		fail "Gate-Assertions ${W}x${H}"
	fi
	if rg -q 'CSRETRO_LOC_MISSING' "${LOG}" 2>/dev/null || rg -q 'CSRETRO_LOC_MISSING' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
		rg 'CSRETRO_LOC_' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | head -40 >&2 || true
		stop_engine "${XASH_PID}"
		fail "Localization ${W}x${H}"
	fi

	mkdir -p "${SHOT_DIR}"
	local newest=""
	newest="$(find "${RUN}/cstrike/scrshots" "${RUN}" -maxdepth 3 \( -name '*.png' -o -name '*.tga' -o -name '*.bmp' \) -size +20k -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2- || true)"
	if [[ -z "${newest}" ]]; then
		newest="$(find "${RUN}/cstrike/scrshots" -name '_shot*.png' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2- || true)"
	fi
	if [[ -n "${newest}" && -f "${newest}" ]]; then
		cp -a "${newest}" "${shot}"
		echo "SHOT_SRC=${newest} bytes=$(stat -c%s "${shot}")"
	else
		echo "WARN: kein Engine-Screenshot ${W}x${H}" >&2
	fi

	stop_engine "${XASH_PID}"
	echo "OPTIONS_VIDEO_GATE PASS ${W}x${H} shot=${shot}"
}

RES_LIST=("640x480" "800x600" "1024x768" "1366x768")
if [[ -n "${CSRETRO_GATE_RES:-}" ]]; then
	RES_LIST=("${CSRETRO_GATE_RES}")
fi

for res in "${RES_LIST[@]}"; do
	W="${res%x*}"
	H="${res#*x}"
	run_one "${W}" "${H}"
done

echo "OPTIONS_VIDEO_GATE PASS all resolutions"
echo "Shots: ${SHOT_DIR}"
rg -n 'CSRETRO_LOC_|CSRETRO_VIDEO_CVAR|CSRETRO_VIDEO_GATE_OK|brightness|gamma|gl_vsync|fullscreen' \
	"${RUN}"/gamescope-options-video-gate-*.log 2>/dev/null | head -80 || true
