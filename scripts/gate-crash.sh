# shellcheck shell=bash
# Shared crash / unclean-exit checks for VGUI gates.
# Source after setting LOG / optional CSRETRO_GAMESCOPE_LOG.
#
# Engine quit contract (used by gates): menu issues ClientCmd("quit"), then the
# shell waits on the PID and requires exit status 0. Do not SIGTERM as a
# substitute for in-engine quit.

csretro_gate_crash_pattern() {
	# Extended: SIGABRT/signal 6 and sanitizer runtime, not only SIGSEGV.
	printf '%s' 'Crash: signal|signal [0-9]|Segmentation fault|Aborted|Abort trap|double free|invalid pointer|corrupted|AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:'
}

# Scan log files for crash signatures. Args: log paths.
csretro_gate_logs_indicate_crash() {
	local pat
	pat="$(csretro_gate_crash_pattern)"
	local f
	for f in "$@"; do
		[[ -f "${f}" ]] || continue
		if rg -q "${pat}" "${f}" 2>/dev/null; then
			return 0
		fi
	done
	return 1
}

# Wait for an engine that already issued ClientCmd quit (or is exiting).
# Usage: csretro_gate_wait_quit PID [timeout_sec] LOG [EXTRA_LOG...]
csretro_gate_wait_quit() {
	local pid="$1"
	local timeout="${2:-30}"
	shift 2
	local logs=("$@")
	local i ec=0

	if [[ -z "${pid}" ]]; then
		echo "csretro_gate_wait_quit: missing pid" >&2
		return 1
	fi

	for i in $(seq 1 "${timeout}"); do
		if ! kill -0 "${pid}" 2>/dev/null; then
			set +e
			wait "${pid}" 2>/dev/null
			ec=$?
			set -e
			if csretro_gate_logs_indicate_crash "${logs[@]}"; then
				echo "csretro_gate_wait_quit: crash signature in logs (exit=${ec})" >&2
				rg -n "$(csretro_gate_crash_pattern)" "${logs[@]}" 2>/dev/null | head -40 >&2 || true
				return 1
			fi
			if [[ "${ec}" -ne 0 ]]; then
				echo "csretro_gate_wait_quit: unclean exit status ${ec}" >&2
				return 1
			fi
			echo "csretro_gate_wait_quit: OK exit=0"
			return 0
		fi
		sleep 1
	done

	echo "csretro_gate_wait_quit: timeout — process still alive" >&2
	return 1
}
