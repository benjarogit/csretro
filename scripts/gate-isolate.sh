# shellcheck shell=bash
# Isolated BASEDIR for automated gates. Never writes GameData or build/run.
#
# Usage:
#   source "${ROOT}/scripts/gate-isolate.sh"
#   csretro_gate_isolate_begin "keyboard"
#   RUN="${CSRETRO_RUN_DIR}"
#   # ... gate uses $RUN only ...
#   csretro_gate_isolate_note
#
# Override: CSRETRO_GATE_ISOLATE_DIR=/tmp/foo  (still not build/run unless forced)

csretro_gate_shared_run() {
	printf '%s\n' "${ROOT}/build/run"
}

csretro_gate_isolate_begin() {
	local tag="${1:?gate tag}"
	local shared isolate
	shared="$(csretro_gate_shared_run)"
	if [[ -n "${CSRETRO_GATE_ISOLATE_DIR:-}" ]]; then
		isolate="${CSRETRO_GATE_ISOLATE_DIR}"
	else
		isolate="${ROOT}/build/run-gate/${tag}"
	fi
	case "${isolate}" in
		"${shared}"|"${shared}/")
			echo "gate-isolate: refusing to use shared play BASEDIR ${shared}" >&2
			return 1
			;;
	esac
	if [[ -n "${CSRETRO_GAMEDATA:-}" && "${isolate}" == "${CSRETRO_GAMEDATA}" ]]; then
		echo "gate-isolate: refusing to use GameData as BASEDIR" >&2
		return 1
	fi

	rm -rf "${isolate}"
	mkdir -p "${isolate}/cstrike/dlls" "${isolate}/cstrike/cl_dlls" \
		"${isolate}/cstrike/resource" "${isolate}/cstrike/gfx/shell" \
		"${isolate}/valve" "${isolate}/cfg"

	export CSRETRO_GATE_ISOLATE_DIR="${isolate}"
	export CSRETRO_GATE_ISOLATE_TAG="${tag}"
	export CSRETRO_RUN_DIR="${isolate}"

	echo "CSRETRO_GATE_ISOLATE tag=${tag} basedir=${isolate}"
	echo "CSRETRO_GATE_ISOLATE play_basedir=${shared} (untouched)"
}

# Overlays + binaries into isolated BASEDIR only. GameData stays read-only.
csretro_gate_isolate_stage_runtime() {
	local isolate="${CSRETRO_RUN_DIR:?}"
	local menu="${CSRETRO_MENU_SO:?}"
	local client="${CSRETRO_CLIENT_SO:?}"
	local gamedll="${CSRETRO_GAMEDLL_SO:?}"
	local eng="${CSRETRO_ENGINE_OUT:?}"

	cp -a "${gamedll}" "${isolate}/cstrike/dlls/cs_amd64.so"
	cp -a "${client}" "${isolate}/cstrike/cl_dlls/client_amd64.so"
	cp -a "${menu}" "${isolate}/menu_amd64.so"
	ln -sfn "${menu}" "${isolate}/libmenu.so"
	ln -sfn "${eng}/engine/libxash.so" "${isolate}/libxash.so"
	ln -sfn "${eng}/ref/gl/libref_gl.so" "${isolate}/libref_gl.so"
	ln -sfn "${eng}/filesystem/filesystem_stdio.so" "${isolate}/filesystem_stdio.so"
	ln -sfn "${eng}/game_launch/xash3d" "${isolate}/xash3d"

	if [[ -d "${ROOT}/data/ui-overrides/cstrike" ]]; then
		cp -a "${ROOT}/data/ui-overrides/cstrike/resource/." "${isolate}/cstrike/resource/" 2>/dev/null || true
		mkdir -p "${isolate}/cstrike/gfx/shell"
		cp -a "${ROOT}/data/ui-overrides/cstrike/gfx/shell/." "${isolate}/cstrike/gfx/shell/" 2>/dev/null || true
		if [[ -f "${ROOT}/scripts/gamedata-env.sh" ]]; then
			# shellcheck source=gamedata-env.sh
			source "${ROOT}/scripts/gamedata-env.sh"
			csretro_stage_valve_loc \
				"${ROOT}/data/ui-overrides/cstrike/resource/csretro_gameui_english.txt" \
				"${isolate}/cstrike/resource/csretro_gameui_english.txt"
		fi
	fi
}

csretro_gate_isolate_note() {
	echo "CSRETRO_GATE_ISOLATE exec_context basedir=${CSRETRO_RUN_DIR}"
	echo "CSRETRO_GATE_ISOLATE exec_context rodir=${XASH3D_RODIR:-}"
	echo "CSRETRO_GATE_ISOLATE exec_files ${CSRETRO_RUN_DIR}/cstrike/cstrike.rc ${CSRETRO_RUN_DIR}/cstrike/autoexec.cfg ${CSRETRO_RUN_DIR}/cstrike/config.cfg ${CSRETRO_RUN_DIR}/cstrike/userconfig.cfg"
}
