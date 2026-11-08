#!/usr/bin/env bash
# Shared helpers for the server toolchain and runtime fetchers.
#
# This file is sourced, not executed. The caller sets ROOT before calling
# anything; the cache and lockfile locations can be overridden through
# CSRETRO_CACHE_DIR, CACHE, LOCKFILE and DOWNLOADS.

: "${ROOT:?ROOT must be set before sourcing scripts/lib/fetch.sh}"
CACHE="${CACHE:-${CSRETRO_CACHE_DIR:-$ROOT/.cache}}"
LOCKFILE="${LOCKFILE:-$ROOT/versions.lock.json}"
DOWNLOADS="${DOWNLOADS:-$CACHE/downloads}"

# lock_value <section> <key> <field>
lock_value() {
    python3 - "$LOCKFILE" "$1" "$2" "$3" <<'PY'
import json
import sys

path, section, key, field = sys.argv[1:5]
with open(path, encoding="utf-8") as handle:
    data = json.load(handle)
print(data[section][key][field])
PY
}

# fetch_archive <section> <key>
# Downloads the archive once, verifies its SHA-256 and prints its path.
fetch_archive() {
    local section="$1" key="$2" url sha file actual
    url="$(lock_value "$section" "$key" url)"
    sha="$(lock_value "$section" "$key" sha256)"
    file="$DOWNLOADS/$(basename "${url%%\?*}")"

    mkdir -p "$DOWNLOADS"
    if [[ -f "$file" ]] && [[ "$(sha256sum "$file" | awk '{print $1}')" == "$sha" ]]; then
        printf '==> %s: cached %s\n' "$key" "$(basename "$file")" >&2
    else
        printf '==> %s: downloading %s\n' "$key" "$(basename "$file")" >&2
        curl -fsSL --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
        actual="$(sha256sum "$file" | awk '{print $1}')"
        if [[ "$actual" != "$sha" ]]; then
            printf 'ERROR: %s has sha256 %s, expected %s\n' "$file" "$actual" "$sha" >&2
            rm -f "$file"
            exit 1
        fi
    fi

    printf '%s\n' "$file"
}
