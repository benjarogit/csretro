#!/usr/bin/env bash
# Fetch the server runtime packages.
#
# Reads the vendored ReHLDS, ReGameDLL_CS, Metamod-R, AMX Mod X and ReAPI
# archives under server/vendor/dist/, verifies every SHA-256 against
# versions.lock.json and prints their paths.
#
# Prints shell assignments on stdout:
#
#   eval "$(scripts/fetch-server-runtime.sh)"
#   ls "$RUNTIME_ARCHIVES_DIR"
#
# With --stage <dir> the verified archives are copied into <dir> so a Docker
# build can unpack them without network access.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

# shellcheck source=scripts/lib/fetch.sh
. "$ROOT/scripts/lib/fetch.sh"

stage=""
while (($# > 0)); do
    case "$1" in
        --stage) stage="${2:?missing value}"; shift ;;
        -h|--help)
            printf 'usage: scripts/fetch-server-runtime.sh [--stage <dir>]\n'
            exit 0
            ;;
        *)
            printf 'unknown option: %s\n' "$1" >&2
            exit 2
            ;;
    esac
    shift
done

archives=()
for entry in \
    "server_toolchain amxmodx" \
    "server_toolchain amxmodx_cstrike" \
    "server_toolchain reapi" \
    "server_runtime rehlds" \
    "server_runtime regamedll" \
    "server_runtime metamod_r"; do
    read -r section key <<<"$entry"
    archives+=("$(fetch_archive "$section" "$key")")
done

if [[ -n "$stage" ]]; then
    mkdir -p "$stage"
    cp -f "${archives[@]}" "$stage/"
    printf 'staged %d archives in %s\n' "${#archives[@]}" "$stage" >&2
fi

printf 'RUNTIME_ARCHIVES_DIR=%q\n' "$(dirname -- "${archives[0]}")"
printf 'RUNTIME_STEAMCMD_IMAGE=%q\n' "$(lock_value server_runtime steamcmd image)"
