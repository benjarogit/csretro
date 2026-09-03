# shellcheck shell=bash
# Privates X11 ohne Fenster auf dem Benutzer-Desktop.
# gamescope --backend headless: kein Raise, kein Fokusdiebstahl.
# CSRETRO_FOREGROUND=1: altes Verhalten (Fenster auf $DISPLAY).

csretro_headless_x11_prepare() {
	CSRETRO_HEADLESS=0
	CSRETRO_GAMESCOPE_PID=""
	if [[ "${CSRETRO_FOREGROUND:-0}" == 1 ]]; then
		export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
		export DISPLAY="${DISPLAY:-:0}"
		# Native desktop: Gamescope WSI implicit Vulkan layer must stay off
		# (otherwise zenity "CreateSwapchainKHR… Hooking has failed" + GL crashes).
		unset ENABLE_GAMESCOPE_WSI 2>/dev/null || true
		export DISABLE_GAMESCOPE_WSI=1
		return 0
	fi
	command -v gamescope >/dev/null 2>&1 || {
		echo "headless-x11: gamescope fehlt. CSRETRO_FOREGROUND=1 für sichtbares Fenster." >&2
		return 1
	}
	CSRETRO_HEADLESS=1
	# Caller may set CSRETRO_GAMESCOPE_LOG; do not wipe it.
	if [[ -z "${CSRETRO_GAMESCOPE_LOG:-}" ]]; then
		CSRETRO_GAMESCOPE_LOG="${CSRETRO_RUN_DIR:-.}/gamescope.log"
	fi
	mkdir -p "$(dirname "${CSRETRO_GAMESCOPE_LOG}")"
	: >"${CSRETRO_GAMESCOPE_LOG}"
}

csretro_headless_x11_wrap() {
	# exec: Aufrufer macht `wrap … &` und $! ist gamescope bzw. xash.
	if [[ "${CSRETRO_HEADLESS:-0}" != 1 ]]; then
		exec "$@"
	fi
	local W="${CSRETRO_GAMESCOPE_W:-640}"
	local H="${CSRETRO_GAMESCOPE_H:-480}"
	local envcmd=(env SDL_VIDEODRIVER=x11)
	# gamescope startet Kind mit `env` — LD_PRELOAD/ASan explizit mitgeben.
	[[ -n "${LD_PRELOAD:-}" ]] && envcmd+=(LD_PRELOAD="${LD_PRELOAD}")
	[[ -n "${ASAN_OPTIONS:-}" ]] && envcmd+=(ASAN_OPTIONS="${ASAN_OPTIONS}")
	[[ -n "${UBSAN_OPTIONS:-}" ]] && envcmd+=(UBSAN_OPTIONS="${UBSAN_OPTIONS}")
	exec gamescope \
		--backend headless \
		-W "${W}" -H "${H}" \
		-w "${W}" -h "${H}" \
		-- \
		"${envcmd[@]}" \
		"$@"
}

csretro_headless_x11_wait_display() {
	if [[ "${CSRETRO_HEADLESS:-0}" != 1 ]]; then
		return 0
	fi
	local i disp
	for i in $(seq 1 50); do
		if [[ -f "${CSRETRO_GAMESCOPE_LOG}" ]]; then
			disp="$(rg -o 'Starting Xwayland on :[0-9]+' "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null | tail -n1 | awk '{print $NF}')"
			if [[ -n "${disp}" ]]; then
				export DISPLAY="${disp}"
				export SDL_VIDEODRIVER=x11
				return 0
			fi
		fi
		sleep 0.1
	done
	echo "headless-x11: kein Xwayland von gamescope (Log: ${CSRETRO_GAMESCOPE_LOG})" >&2
	return 1
}

csretro_headless_x11_stop() {
	if [[ -n "${CSRETRO_GAMESCOPE_PID:-}" ]] && kill -0 "${CSRETRO_GAMESCOPE_PID}" >/dev/null 2>&1; then
		kill -TERM -- -"${CSRETRO_GAMESCOPE_PID}" >/dev/null 2>&1 || kill -TERM "${CSRETRO_GAMESCOPE_PID}" >/dev/null 2>&1 || true
		sleep 0.4
		kill -KILL -- -"${CSRETRO_GAMESCOPE_PID}" >/dev/null 2>&1 || kill -KILL "${CSRETRO_GAMESCOPE_PID}" >/dev/null 2>&1 || true
	fi
}
