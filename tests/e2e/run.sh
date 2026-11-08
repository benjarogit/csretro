#!/usr/bin/env bash
# End-to-end test for the CSRETRO test server.
#
# Builds the server image from the staged runtime archives and the freshly
# built module, starts it with Docker Compose, waits for the healthcheck and
# runs tests/e2e/test_server.py against it. The container and its volumes are
# removed on every exit path.
#
# This test needs Docker and network access: the image build downloads HLDS
# App 90 through SteamCMD. It is a local and CI (G10) test, not part of
# scripts/verify.sh.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

PROJECT="${CSR_E2E_PROJECT:-csretro-e2e}"
HOST_PORT="${CSR_E2E_HOST_PORT:-27015}"
RCON_PASSWORD="${CSR_E2E_RCON_PASSWORD:-csretro-e2e}"
MAP="${CSR_E2E_MAP:-de_dust2}"

CONTEXT="$ROOT/dist/testserver-context"
STAGE="$ROOT/dist/testserver-stage"
LOG="$CONTEXT/server.log"

log() { printf '==> %s\n' "$*"; }
fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail "docker is not installed"
docker compose version >/dev/null 2>&1 || fail "docker compose is not available"
docker info >/dev/null 2>&1 || fail "cannot talk to the Docker daemon"

compose() {
    docker compose -p "$PROJECT" -f "$CONTEXT/docker-compose.yml" "$@"
}

cleanup() {
    local status=$?
    if [[ -f "$CONTEXT/docker-compose.yml" ]]; then
        compose down --volumes --remove-orphans >/dev/null 2>&1 || true
    fi
    if ((status != 0)) && [[ -f "$LOG" ]]; then
        printf '\n--- server log (last 80 lines) ---\n' >&2
        tail -n 80 "$LOG" >&2
        printf '%s\n' '--- end of log ---' >&2
    fi
}
trap cleanup EXIT

log "staging runtime archives, module, plugin and test assets"
rm -rf "$STAGE"
"$ROOT/scripts/build-server.sh" --stage "$STAGE"

log "assembling the Docker build context in ${CONTEXT#"$ROOT"/}"
rm -rf "$CONTEXT"
mkdir -p "$CONTEXT"
cp -a "$ROOT/server/pack/." "$CONTEXT/"
cp -a "$STAGE/." "$CONTEXT/"

steamcmd_image="$(sed -n 's/^# steamcmd_image: //p' "$CONTEXT/staging-manifest.txt")"
[[ -n "$steamcmd_image" ]] || fail "staging manifest has no steamcmd image"

export CSR_STEAMCMD_IMAGE="$steamcmd_image"
export CSR_HOST_PORT="$HOST_PORT"
export CSR_MAP="$MAP"
export CSR_RCON_PASSWORD="$RCON_PASSWORD"

log "building and starting the test server on udp/$HOST_PORT"
compose up --detach --build --wait --wait-timeout 240

log "collecting the server log"
compose logs --no-color --no-log-prefix server >"$LOG"

log "running protocol and log checks"
python3 "$ROOT/tests/e2e/test_server.py" \
    --log "$LOG" \
    --port "$HOST_PORT" \
    --password "$RCON_PASSWORD" \
    --map "$MAP"

log "e2e passed"
