#!/usr/bin/env bash
# Verifies the repository UTF-8 sources and Valve-compatible staged files.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT}/scripts/gamedata-env.sh"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

fail() {
	echo "localization-encoding-gate: $*" >&2
	exit 1
}

check_source() {
	local source_file="$1"
	[[ "$(head -c 3 "${source_file}" | od -An -t x1 | tr -d ' \n')" != "efbbbf" ]] || fail "source has UTF-8 BOM: ${source_file}"
	iconv -f UTF-8 -t UTF-8 "${source_file}" >/dev/null || fail "source is not valid UTF-8: ${source_file}"
	[[ "$(head -c 6 "${source_file}")" == '"lang"' ]] || fail "source does not begin with \"lang\": ${source_file}"
}

check_staged() {
	local staged_file="$1"
	local prefix
	prefix="$(head -c 14 "${staged_file}" | od -An -t x1 | tr -d ' \n')"
	[[ "${prefix}" == "fffe22006c0061006e0067002200" ]] || fail "staged prefix is invalid: ${staged_file}"
	[[ "$(head -c 4 "${staged_file}" | od -An -t x1 | tr -d ' \n')" != "fffefffe" ]] || fail "staged file has multiple BOMs: ${staged_file}"
	iconv -f UTF-16LE -t UTF-8 "${staged_file}" >/dev/null || fail "staged file is not valid UTF-16LE: ${staged_file}"
	[[ "$(tail -c +3 "${staged_file}" | iconv -f UTF-16LE -t UTF-8 | head -c 6)" == '"lang"' ]] || fail "staged file does not decode to \"lang\": ${staged_file}"
}

for language in english german; do
	source_file="${ROOT}/data/ui-overrides/cstrike/resource/csretro_gameui_${language}.txt"
	staged_file="${TMP}/csretro_gameui_${language}.txt"
	check_source "${source_file}"
	csretro_stage_valve_loc "${source_file}" "${staged_file}"
	check_staged "${staged_file}"
done

echo "CSRETRO_LOC_ENCODING_GATE OK"