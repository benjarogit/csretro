# shellcheck shell=bash
# Strip gate/3C fixtures from the shared play BASEDIR (build/run).
# Does not touch GameData. Backs up once to cstrike/.gate-legacy/.

csretro_play_gate_marker_re() {
	printf '%s\n' 'CSRETRO_(3C_CFG_LOADED|MENU_CFG_LOADED|V1POC_CFG|VGUI_SHUTDOWN_GATE_CFG|OPTIONS_[A-Z_]*_CFG)|\+csretro_'
}

csretro_play_is_3c_fixture() {
	local f="$1"
	[[ -f "${f}" ]] || return 1
	rg -q "$(csretro_play_gate_marker_re)" "${f}" 2>/dev/null
}

csretro_play_seed_cs_defaults() {
	local run="${1:?}"
	local rodir="${2:-}"
	local reason="${3:-init}"
	local cs="${run}/cstrike"
	local defaults="${rodir}/cstrike/gfx/shell/kb_def.lst"
	local overlay="${rodir}/cstrike/gfx/shell/kb_def_overlay.lst"
	local tmp="${cs}/config.cfg.csretro-new"

	if [[ ! -f "${defaults}" ]]; then
		echo "CSRETRO_PLAY_SANITIZE skip default seed (${defaults} fehlt)"
		return 1
	fi

	mkdir -p "${cs}"
	{
		printf '%s\n' '// CS Retro initialized keyboard defaults.'
		printf '%s\n' '// User changes after this point are preserved by Host_WriteConfig.'
		printf '%s\n' 'unbindall'
		{
			sed -n 's/^[[:space:]]*"\([^"]*\)"[[:space:]]*"\([^"]*\)".*/bind "\1" "\2"/p' "${defaults}"
			if [[ -f "${overlay}" ]]; then
				sed -n 's/^[[:space:]]*"\([^"]*\)"[[:space:]]*"\([^"]*\)".*/bind "\1" "\2"/p' "${overlay}"
			fi
		} | rg -v '^bind "ESCAPE" ' | awk '!seen[$0]++'
		printf '%s\n' 'bind "ESCAPE" "cancelselect"'
		printf '%s\n' 'exec userconfig.cfg'
	} > "${tmp}"
	mv -f "${tmp}" "${cs}/config.cfg"
	echo "CSRETRO_PLAY_SANITIZE config.cfg (${reason} → CS keyboard defaults)"
}

csretro_play_config_needs_cs_defaults() {
	local cfg="$1"
	[[ ! -s "${cfg}" ]] && return 0
	rg -q "$(csretro_play_gate_marker_re)" "${cfg}" 2>/dev/null && return 0
	if ! rg -q '^bind "w" "\+forward"' "${cfg}" 2>/dev/null &&
		! rg -q '^bind "a" "\+moveleft"' "${cfg}" 2>/dev/null &&
		! rg -q '^bind "d" "\+moveright"' "${cfg}" 2>/dev/null &&
		rg -q '^bind "UPARROW" "\+forward"' "${cfg}" 2>/dev/null; then
		return 0
	fi
	return 1
}

csretro_play_sanitize_usercfg() {
	local run="${1:?}"
	local rodir="${2:-}"
	local cs="${run}/cstrike"
	local bak="${cs}/.gate-legacy"
	local changed=0

	mkdir -p "${cs}"

	if [[ -f "${cs}/autoexec.cfg" ]] && rg -q "$(csretro_play_gate_marker_re)" "${cs}/autoexec.cfg" 2>/dev/null; then
		mkdir -p "${bak}"
		cp -a "${cs}/autoexec.cfg" "${bak}/autoexec.cfg"
		# Keep non-marker lines (developer, user cvars). Drop gate echoes and 3C copies.
		if rg -q 'CSRETRO_3C_CFG_LOADED|\+csretro_fwd' "${cs}/autoexec.cfg" 2>/dev/null; then
			printf '%s\n' '# CS Retro user autoexec (gate/3C fixture moved to .gate-legacy)' > "${cs}/autoexec.cfg"
		else
			rg -v "$(csretro_play_gate_marker_re)" "${cs}/autoexec.cfg" > "${cs}/autoexec.cfg.play" || true
			mv -f "${cs}/autoexec.cfg.play" "${cs}/autoexec.cfg"
		fi
		changed=1
		echo "CSRETRO_PLAY_SANITIZE autoexec.cfg (removed gate markers)"
	fi

	if csretro_play_is_3c_fixture "${cs}/userconfig.cfg"; then
		mkdir -p "${bak}"
		cp -a "${cs}/userconfig.cfg" "${bak}/userconfig.cfg"
		printf '%s\n' \
			'// CS Retro userconfig — user bindings only.' \
			'// Automated 3C/gate aliases live in build/run-3c, not here.' \
			> "${cs}/userconfig.cfg"
		changed=1
		echo "CSRETRO_PLAY_SANITIZE userconfig.cfg (3C fixture → ${bak}/userconfig.cfg)"
	fi

	if csretro_play_config_needs_cs_defaults "${cs}/config.cfg"; then
		mkdir -p "${bak}"
		[[ -f "${cs}/config.cfg" ]] && cp -a "${cs}/config.cfg" "${bak}/config.cfg"
		csretro_play_seed_cs_defaults "${run}" "${rodir}" "missing/HL-fallback/gate"
		changed=1
	elif [[ -f "${cs}/config.cfg" ]] && rg -q '\+csretro_|CSRETRO_3C_' "${cs}/config.cfg" 2>/dev/null; then
		mkdir -p "${bak}"
		cp -a "${cs}/config.cfg" "${bak}/config.cfg"
		python3 - "${cs}/config.cfg" <<'PY'
import re, sys
path = sys.argv[1]
repl = (
    ("+csretro_fwd", "+forward"),
    ("+csretro_rel", "+reload"),
    ("+csretro_atk", "+attack"),
    ("+csretro_jump", "+jump"),
    ("+csretro_duck", "+duck"),
)
text = open(path, "r", encoding="utf-8", errors="replace").read()
out = []
for line in text.splitlines(True):
    nl = "\n" if line.endswith("\n") else ""
    s = line.rstrip("\n")
    slot = re.match(r'(bind\s+"[^"]+"\s+")(slot[123]); echo CSRETRO_3C_[^"]+(")', s)
    if slot:
        out.append(f"{slot.group(1)}{slot.group(2)}{slot.group(3)}{nl}")
        continue
    if re.search(r"3c-actions\.cfg|CSRETRO_3C_", s):
        continue
    for dummy, real in repl:
        s = s.replace(dummy, real)
    out.append(s + nl)
open(path, "w", encoding="utf-8").writelines(out)
PY
		changed=1
		echo "CSRETRO_PLAY_SANITIZE config.cfg (replaced +csretro_* with catalog commands)"
	fi

	# 3C wrote: exec autoexec.cfg + stuffcmds. Play should exec the user archive.
	if [[ -f "${cs}/cstrike.rc" ]]; then
		if rg -qx 'exec autoexec.cfg' "${cs}/cstrike.rc" && ! rg -q 'exec config.cfg' "${cs}/cstrike.rc"; then
			mkdir -p "${bak}"
			cp -a "${cs}/cstrike.rc" "${bak}/cstrike.rc"
			printf '%s\n' 'exec config.cfg' 'exec userconfig.cfg' 'stuffcmds' > "${cs}/cstrike.rc"
			changed=1
			echo "CSRETRO_PLAY_SANITIZE cstrike.rc (config.cfg + userconfig.cfg, no gate autoexec)"
		fi
	fi

	if [[ "${changed}" -eq 0 ]]; then
		echo "CSRETRO_PLAY_SANITIZE clean (no gate/3C fixtures in ${cs})"
	fi
}

csretro_play_print_exec_context() {
	local run="${1:?}"
	local rodir="${2:-}"
	local cs="${run}/cstrike"
	echo "CSRETRO_PLAY_CONFIG basedir=${run}"
	echo "CSRETRO_PLAY_CONFIG rodir=${rodir}"
	echo "CSRETRO_PLAY_CONFIG game=cstrike"
	echo "CSRETRO_PLAY_CONFIG rc=${cs}/cstrike.rc"
	echo "CSRETRO_PLAY_CONFIG autoexec=${cs}/autoexec.cfg"
	echo "CSRETRO_PLAY_CONFIG config.cfg=${cs}/config.cfg"
	echo "CSRETRO_PLAY_CONFIG userconfig.cfg=${cs}/userconfig.cfg"
	echo "CSRETRO_PLAY_CONFIG engine_exec: cstrike.rc → config.cfg → userconfig.cfg → userconfigd (host.c)"
	echo "CSRETRO_PLAY_CONFIG BIND_AUDIT reads the same config.cfg path (XASH3D_BASEDIR/cstrike/config.cfg)"
}
