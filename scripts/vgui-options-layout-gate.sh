#!/usr/bin/env bash
# Options Adaptive Layout + native edge/corner resize gate.
# Usage: ./scripts/build-menu.sh && ./scripts/vgui-options-layout-gate.sh
# Optional: CSRETRO_FOREGROUND=1, CSRETRO_GATE_RES=640x480
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-isolate.sh"

ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
SHOT_DIR="${ROOT}/build/options-layout-shots"
MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
cd "${ROOT}"

fail() { echo "OPTIONS_LAYOUT_GATE FAIL: $*" >&2; exit 1; }

[[ -f "${MENU}" ]] || fail "menu fehlt"
[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
MENU="$(readlink -f "${MENU}")"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
	command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt"
fi

export CSRETRO_ENGINE_OUT="${ENG}" CSRETRO_CLIENT_SO="${CLIENT}" \
	CSRETRO_GAMEDLL_SO="${GAMEDLL}" CSRETRO_MENU_SO="${MENU}"
csretro_gate_isolate_begin "layout" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
mkdir -p "${SHOT_DIR}"

cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
echo CSRETRO_OPTIONS_LAYOUT_GATE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
csretro_gate_isolate_note
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_OPTIONS_LAYOUT_GATE=1
export CSRETRO_VGUI_METRICS_DUMP=1
export CSRETRO_MENU_SO="${MENU}"
export CSRETRO_MENU_SHA256="$(sha256sum "${MENU}" | awk '{print $1}')"
unset CSRETRO_V1POC CSRETRO_OPTIONS_AUTO CSRETRO_OPTIONS_GATE \
	CSRETRO_OPTIONS_AUDIO_GATE CSRETRO_OPTIONS_VIDEO_GATE \
	CSRETRO_OPTIONS_VIDEO_MODE_SAFETY CSRETRO_OPTIONS_KEYBOARD_GATE \
	CSRETRO_OPTIONS_KEYBOARD_CAPTURE_PHYS 2>/dev/null || true
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
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-options-gate-${W}x${H}.log"
	local shot="${SHOT_DIR}/csretro-options-layout-${W}x${H}.png"

	csretro_headless_x11_prepare || fail "gamescope"
	rm -f "${LOG}"
	: >"${CSRETRO_GAMESCOPE_LOG}"
	rm -rf "${RUN}/cstrike/scrshots"
	mkdir -p "${RUN}/cstrike/scrshots"
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
		+maxplayers 2 +sv_lan 1 +exec autoexec.cfg \
		>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	local XASH_PID=$!
	CSRETRO_GAMESCOPE_PID="${XASH_PID}"
	set -e
	cd "${ROOT}"

	csretro_headless_x11_wait_display || { stop_engine "${XASH_PID}"; fail "X11 ${W}x${H}"; }

	wait_log 'CSRETRO_OPTIONS_VISIBLE' 40 || { stop_engine "${XASH_PID}"; fail "VISIBLE ${W}x${H}"; }
	wait_log 'CSRETRO_LAYOUT_GATE_DONE' 60 || { stop_engine "${XASH_PID}"; fail "GATE_DONE ${W}x${H}"; }
	sleep 1

	if rg -q 'CSRETRO_LAYOUT_GATE_FAIL' "${LOG}" 2>/dev/null || rg -q 'CSRETRO_LAYOUT_GATE_FAIL' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
		rg 'CSRETRO_LAYOUT_GATE_FAIL|CSRETRO_LAYOUT_' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | head -50 >&2 || true
		stop_engine "${XASH_PID}"
		fail "Gate-Assertions ${W}x${H}"
	fi
	for need in 'classic_list_512x406' 'grow_list' 'grow_list_fits_page' 'classic_after_shrink' \
		'sizeable_enabled' 'resize_grips_all' 'resize_top_left_boundary_anchor' \
		'resize_workspace_clamp' 'resize_min_clamp' \
		'querybox_after_resize' 'persist_roundtrip' 'clamp_640_full' 'clamp_offscreen_full' \
		'mouse_classic_after_grow' 'geometry_live_without_apply'; do
		if ! rg -q "CSRETRO_LAYOUT_GATE_OK ${need}" "${LOG}" 2>/dev/null &&
			! rg -q "CSRETRO_LAYOUT_GATE_OK ${need}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			stop_engine "${XASH_PID}"
			fail "missing ${need} ${W}x${H}"
		fi
	done

	local newest=""
	mapfile -t _shots < <(find "${RUN}/cstrike/scrshots" -maxdepth 1 \( -name '*.png' -o -name '*.tga' -o -name '*.bmp' \) -size +20k | sort)
	if ((${#_shots[@]} >= 1)); then
		newest="${_shots[-1]}"
		cp -a "${newest}" "${shot}"
		echo "SHOT_SRC=${newest}"
	fi
	# Named phases: first three sequential shots ≈ classic / grow / min
	if ((${#_shots[@]} >= 3)); then
		cp -a "${_shots[0]}" "${SHOT_DIR}/csretro-options-layout-${W}x${H}-classic.png"
		cp -a "${_shots[1]}" "${SHOT_DIR}/csretro-options-layout-${W}x${H}-grow.png"
		cp -a "${_shots[2]}" "${SHOT_DIR}/csretro-options-layout-${W}x${H}-min.png"
		echo "SHOT_PHASES classic=${_shots[0]} grow=${_shots[1]} min=${_shots[2]}"
	fi

	stop_engine "${XASH_PID}"
	echo "OPTIONS_LAYOUT_GATE AUTOMATED_OK ${W}x${H} shot=${shot}"
}

verify_restart_restore() {
	# The preceding gate process saved 700x520. Start a clean process at 640x480
	# through the normal restore path (not the in-process layout gate path).
	local W=640 H=480
	export CSRETRO_GAMESCOPE_W="${W}"
	export CSRETRO_GAMESCOPE_H="${H}"
	export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-options-restart-${W}x${H}.log"

	csretro_headless_x11_prepare || fail "gamescope restart-restore"
	rm -f "${LOG}"
	: >"${CSRETRO_GAMESCOPE_LOG}"
	killall -q xash3d 2>/dev/null || true
	sleep 0.2

	unset CSRETRO_OPTIONS_LAYOUT_GATE
	export CSRETRO_OPTIONS_AUTO=1
	export CSRETRO_OPTIONS_LAYOUT_RESTORE_GATE=1
	cd "${RUN}"
	set +e
	csretro_headless_x11_wrap ./xash3d \
		-game cstrike \
		-dll "${GAMEDLL}" \
		-clientlib "${CLIENT}" \
		-menulib "${MENU}" \
		-windowed -width "${W}" -height "${H}" \
		-dev 2 -log \
		+maxplayers 2 +sv_lan 1 +exec autoexec.cfg \
		>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
	local XASH_PID=$!
	CSRETRO_GAMESCOPE_PID="${XASH_PID}"
	set -e
	cd "${ROOT}"

	csretro_headless_x11_wait_display || { stop_engine "${XASH_PID}"; fail "X11 restart-restore"; }
	wait_log 'CSRETRO_LAYOUT_RESTART_RESTORE' 40 || {
		stop_engine "${XASH_PID}"
		fail "restart-restore marker"
	}
	if ! rg -q 'CSRETRO_LAYOUT_RESTART_RESTORE bounds=0,0 640x480 screen=640x480' "${LOG}" 2>/dev/null &&
		! rg -q 'CSRETRO_LAYOUT_RESTART_RESTORE bounds=0,0 640x480 screen=640x480' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
		rg 'CSRETRO_UI_GEOM_LOAD|CSRETRO_LAYOUT_RESTART_RESTORE' "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null >&2 || true
		stop_engine "${XASH_PID}"
		fail "restart-restore bounds"
	fi
	stop_engine "${XASH_PID}"
	echo "OPTIONS_LAYOUT_RESTART_RESTORE AUTOMATED_OK 700x520 -> 640x480"
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

verify_restart_restore

echo "OPTIONS_LAYOUT_GATE AUTOMATED_OK all resolutions"
echo "OPTIONS_LAYOUT_GATE STATUS: NATIVE_RESIZE_AUTOMATED_OK"
echo "See docs/PHASE3M-LAYOUT.md"
echo "Shots: ${SHOT_DIR}"
rg -n 'CSRETRO_LAYOUT_GATE_OK|CSRETRO_LAYOUT_GATE_FAIL|CSRETRO_LAYOUT_MIN|CSRETRO_LAYOUT_LIST' \
	"${RUN}"/gamescope-options-gate-*.log 2>/dev/null | head -80 || true
