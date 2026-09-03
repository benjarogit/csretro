#!/usr/bin/env bash
# Options Video Mode-Safety Gate (Cancel/Timeout always; FS/Borderless opt-in).
# Prefers native desktop (CSRETRO_FOREGROUND=1) — Gamescope WSI is a separate issue.
# Usage:
#   CSRETRO_FOREGROUND=1 CSRETRO_MODE_SAFETY_ALLOW_FS=1 CSRETRO_VID_REINIT_TRACE=1 \
#     ./scripts/vgui-options-video-mode-safety.sh
# Wall-clock 10s Overall (no ExpireConfirmNow):
#   CSRETRO_FOREGROUND=1 CSRETRO_MODE_SAFETY_ALLOW_FS=1 CSRETRO_MODE_SAFETY_WALLCLOCK=1 \
#     CSRETRO_VID_REINIT_TRACE=1 DISABLE_GAMESCOPE_WSI=1 \
#     ./scripts/vgui-options-video-mode-safety.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-crash.sh"

RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run-gate/video-mode-safety}"
case "${RUN}" in /*) ;; *) RUN="${ROOT}/${RUN}" ;; esac
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
LOG="${RUN}/engine.log"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
W="${CSRETRO_WIDTH:-1280}"
H="${CSRETRO_HEIGHT:-720}"
cd "${ROOT}"

fail() { echo "OPTIONS_VIDEO_MODE_SAFETY FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
if nm -D "${MENU}" 2>/dev/null | rg -q '__asan_'; then
	ASAN_SO="${CSRETRO_ASAN_SO:-/usr/lib/clang/22/lib/linux/libclang_rt.asan-x86_64.so}"
	[[ -f "${ASAN_SO}" ]] || fail "sanitize-menu braucht ASan-Runtime (${ASAN_SO})"
	export LD_PRELOAD="${ASAN_SO}${LD_PRELOAD:+:${LD_PRELOAD}}"
	export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=0:abort_on_error=1}"
	export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=0}"
fi
MENU="$(readlink -f "${MENU}")"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

if [[ "${CSRETRO_FOREGROUND:-}" == "" && -n "${DISPLAY:-}" ]]; then
	export CSRETRO_FOREGROUND=1
	echo "MODE_SAFETY: defaulting CSRETRO_FOREGROUND=1 (native; Gamescope WSI separated)"
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

cat > "${RUN}/cstrike/autoexec.cfg" <<EOF
developer 2
fullscreen 0
width ${W}
height ${H}
gl_vsync 1
echo CSRETRO_OPTIONS_VIDEO_MODE_SAFETY_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_OPTIONS_VIDEO_MODE_SAFETY=1
export CSRETRO_VID_REINIT_TRACE="${CSRETRO_VID_REINIT_TRACE:-1}"
if [[ "${CSRETRO_MODE_SAFETY_WALLCLOCK:-0}" == 1 ]]; then
	export CSRETRO_MODE_SAFETY_WALLCLOCK=1
	export CSRETRO_VGUI_METRICS_DUMP=1
	echo "MODE_SAFETY: WALLCLOCK=1 (real ~10s OnTick; no ExpireConfirmNow)"
fi
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE CSRETRO_OPTIONS_AUDIO_GATE \
	CSRETRO_OPTIONS_VIDEO_GATE 2>/dev/null || true
export CSRETRO_RUN_DIR="${RUN}"

SHOT_DIR="${ROOT}/build/options-video-mode-safety-shots"
mkdir -p "${SHOT_DIR}"

copy_named_shot() {
	local tag="$1"
	local named="${RUN}/cstrike/scrshots/mode_safety_${tag}.png"
	local newest=""
	if [[ -f "${named}" ]]; then
		cp -a "${named}" "${SHOT_DIR}/${tag}.png"
		echo "SHOT tag=${tag} src=${named} bytes=$(stat -c%s "${SHOT_DIR}/${tag}.png")"
		return 0
	fi
	newest="$(find "${RUN}/cstrike/scrshots" -maxdepth 1 \( -name '*.png' -o -name '*.tga' \) -size +5k -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2- || true)"
	if [[ -n "${newest}" && -f "${newest}" ]]; then
		cp -a "${newest}" "${SHOT_DIR}/${tag}.png"
		echo "SHOT tag=${tag} src=${newest} (fallback) bytes=$(stat -c%s "${SHOT_DIR}/${tag}.png")"
		return 0
	fi
	echo "WARN: no screenshot for tag=${tag}" >&2
	return 1
}

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
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-options-video-mode-safety.log"

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
XASH_PID=$!
CSRETRO_GAMESCOPE_PID="${XASH_PID}"
set -e
cd "${ROOT}"

if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	csretro_headless_x11_wait_display || {
		kill -KILL "${XASH_PID}" 2>/dev/null || true
		fail "X11"
	}
fi

wait_log 'CSRETRO_OPTIONS_VISIBLE|CSRETRO_MODE_SAFETY_BASELINE' 50 || {
	kill -KILL "${XASH_PID}" 2>/dev/null || true
	fail "Options/Mode-Safety start"
}

DONE_WAIT=90
if [[ "${CSRETRO_MODE_SAFETY_WALLCLOCK:-0}" == 1 ]]; then
	DONE_WAIT=180
	# Capture shots as tags appear (wallclock run stays open ~10s+).
	for _ in $(seq 1 80); do
		if rg -q 'CSRETRO_MODE_SAFETY_SHOT tag=confirm_after_mode' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			[[ -f "${SHOT_DIR}/confirm_after_mode.png" ]] || { sleep 0.6; copy_named_shot confirm_after_mode || true; }
		fi
		if rg -q 'CSRETRO_MODE_SAFETY_SHOT tag=options_after_keep' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			[[ -f "${SHOT_DIR}/options_after_keep.png" ]] || { sleep 0.6; copy_named_shot options_after_keep || true; }
		fi
		if rg -q 'CSRETRO_MODE_SAFETY_SHOT tag=confirm_wallclock' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			[[ -f "${SHOT_DIR}/confirm_wallclock.png" ]] || { sleep 0.6; copy_named_shot confirm_wallclock || true; }
		fi
		if rg -q 'CSRETRO_MODE_SAFETY_DONE' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			break
		fi
		sleep 1
	done
fi

wait_log 'CSRETRO_MODE_SAFETY_DONE' "${DONE_WAIT}" || {
	rg -n 'CSRETRO_MODE_SAFETY_|CSRETRO_VIDEO_CONFIRM|CSRETRO_VID_REINIT|signal|Abort' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | tail -80 >&2 || true
	kill -KILL "${XASH_PID}" 2>/dev/null || true
	fail "MODE_SAFETY_DONE"
}

if [[ "${CSRETRO_MODE_SAFETY_WALLCLOCK:-0}" == 1 ]]; then
	sleep 0.8
	copy_named_shot options_after_timeout || true
	for tag in confirm_after_mode options_after_keep confirm_wallclock options_after_timeout; do
		[[ -f "${SHOT_DIR}/${tag}.png" ]] || fail "missing shot ${tag}"
	done
	# Distinct captures required (same inode/hash = burst collapse).
	hashes="$(md5sum "${SHOT_DIR}"/{confirm_after_mode,options_after_keep,confirm_wallclock,options_after_timeout}.png | awk '{print $1}' | sort -u | wc -l)"
	if [[ "${hashes}" -lt 3 ]]; then
		md5sum "${SHOT_DIR}"/*.png >&2 || true
		fail "shots not distinct enough (unique_md5=${hashes}, need >=3)"
	fi
	# Keep shot must differ from Confirm-after-mode (QueryBox must be gone).
	if cmp -s "${SHOT_DIR}/confirm_after_mode.png" "${SHOT_DIR}/options_after_keep.png"; then
		fail "options_after_keep identical to confirm_after_mode (Confirm still visible?)"
	fi
fi

if rg -q 'CSRETRO_MODE_SAFETY_FAIL|CSRETRO_COMBO_GATE_FAIL' "${LOG}" 2>/dev/null \
	|| rg -q 'CSRETRO_MODE_SAFETY_FAIL|CSRETRO_COMBO_GATE_FAIL' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
	rg 'CSRETRO_MODE_SAFETY_FAIL|CSRETRO_COMBO_GATE_FAIL' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | head -40 >&2 || true
	kill -KILL "${XASH_PID}" 2>/dev/null || true
	fail "Mode-Safety assertions"
fi

echo "==== Reinit trace (expect vid_setmode / R_ChangeDisplaySettings; no VID_CheckChanges twin) ===="
rg -n 'CSRETRO_VID_REINIT' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | head -60 || true

echo "==== Confirm wall-clock timestamps ===="
rg -n 'CSRETRO_VIDEO_CONFIRM' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | head -40 || true

if [[ "${CSRETRO_MODE_SAFETY_WALLCLOCK:-0}" == 1 ]]; then
	# Accept ~10s: 9000..12000 ms between confirm_shown and timeout_fired
	delta="$(cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | rg -o 'timeout_fired_ms=[0-9]+ shown_ms=[0-9]+ delta_ms=([0-9]+)' -r '$1' | tail -1 || true)"
	if [[ -z "${delta}" || ! "${delta}" =~ ^[0-9]+$ ]]; then
		fail "wallclock: missing/invalid timeout_fired delta_ms='${delta}'"
	fi
	if [[ "${delta}" -lt 9000 || "${delta}" -gt 12000 ]]; then
		fail "wallclock: delta_ms=${delta} not in 9000..12000"
	fi
	echo "WALLCLOCK_TIMEOUT_DELTA_MS=${delta} (accept 9000..12000)"
fi

# Mode-safety gate issues quit after DONE — wait for clean exit (catches SIGABRT).
if ! csretro_gate_wait_quit "${XASH_PID}" 45 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}"; then
	if kill -0 "${XASH_PID}" 2>/dev/null; then
		kill -KILL "${XASH_PID}" 2>/dev/null || true
		wait "${XASH_PID}" 2>/dev/null || true
	fi
	csretro_headless_x11_stop
	fail "unclean / timeout quit"
fi
csretro_headless_x11_stop

echo "OPTIONS_VIDEO_MODE_SAFETY PASS (E/F + optional FS if ALLOW_FS=1; WALLCLOCK=${CSRETRO_MODE_SAFETY_WALLCLOCK:-0}) exit=0"
echo "Shots: ${SHOT_DIR}"
ls -la "${SHOT_DIR}" 2>/dev/null || true
