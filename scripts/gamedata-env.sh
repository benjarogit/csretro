# shellcheck shell=bash
# Gemeinsamer Game-Data-Pfad. Niemals Steam-Half-Life als RODIR.

csretro_gamedata_dir() {
    local root="${1:?}"
    if [[ -n "${CSRETRO_GAMEDATA:-}" ]]; then
        printf '%s\n' "${CSRETRO_GAMEDATA}"
        return 0
    fi
    printf '%s\n' "${root}/gamedata"
}

csretro_gamedata_require() {
    local root="${1:?}"
    local map="${2:-de_dust}"
    local gd
    gd="$(csretro_gamedata_dir "${root}")"
    if [[ ! -f "${gd}/cstrike/maps/${map}.bsp" ]]; then
        echo "Game-Data fehlt: ${gd}/cstrike/maps/${map}.bsp" >&2
        echo "Zuerst: python3 ${root}/scripts/bootstrap-gamedata.py" >&2
        return 1
    fi
    case "${gd}" in
        */steamapps/common/Half-Life|*/steamapps/common/Half-Life/)
            echo "XASH3D_RODIR darf nicht die Steam-Half-Life-Installation sein: ${gd}" >&2
            return 1
            ;;
    esac
    printf '%s\n' "${gd}"
}

# Repository-Quellen sind UTF-8 ohne BOM. Valve ILocalize liest dagegen nur
# UTF-16LE mit genau einem BOM im BASEDIR. Nicht anhand von `file` verzweigen:
# eine beschädigte oder versehentlich umkodierte Quelle darf nie durchkopiert
# werden.
csretro_stage_valve_loc() {
    local src="${1:?}" dst="${2:?}" tmp
	[[ -f "${src}" ]] || return 0
	mkdir -p "$(dirname "${dst}")"
    tmp="${dst}.tmp.$$"
	{
		printf '\xff\xfe'
		iconv -f UTF-8 -t UTF-16LE "${src}"
    } > "${tmp}" || {
        rm -f "${tmp}"
        return 1
    }
    mv -f "${tmp}" "${dst}"
}
