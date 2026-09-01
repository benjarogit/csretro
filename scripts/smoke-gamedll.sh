#!/usr/bin/env bash
# Reproduzierbarer GameDLL-Smoke: Xash lädt csretro_gamedll, Map startet.
# Erfolg/Fehler eindeutig über Exitcode und letzte Zeile.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=env.sh
if [[ -f "${ROOT}/scripts/env.sh" ]]; then
    source "${ROOT}/scripts/env.sh"
fi

MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
MODE="${1:-dedicated}"
TIMEOUT_SEC="${CSRETRO_SMOKE_TIMEOUT:-25}"
SANITIZE=0
if [[ "${MODE}" == "--sanitize" ]]; then
    SANITIZE=1
    MODE="${2:-dedicated}"
fi
RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run}"
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
if [[ "${SANITIZE}" -eq 1 ]]; then
    GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-sanitize/cs_amd64.so}"
    ASAN_SO="${CSRETRO_ASAN_SO:-/usr/lib/clang/22/lib/linux/libclang_rt.asan-x86_64.so}"
    [[ -f "${ASAN_SO}" ]] || fail "ASan-Runtime fehlt: ${ASAN_SO}"
    export LD_PRELOAD="${ASAN_SO}${LD_PRELOAD:+:${LD_PRELOAD}}"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=0}"
    export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=0}"
else
    GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
fi
if [[ -f "${ROOT}/scripts/gamedata-env.sh" ]]; then
    # shellcheck source=gamedata-env.sh
    source "${ROOT}/scripts/gamedata-env.sh"
fi

fail() {
    echo "SMOKE FAIL: $*" >&2
    exit 1
}

ok() {
    echo "SMOKE OK: $*"
    exit 0
}

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt (${ENG}/game_launch/xash3d). ./scripts/build-engine.sh"
[[ -f "${GAMEDLL}" ]] || fail "GameDLL fehlt (${GAMEDLL}). ./scripts/build-gamedll.sh"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data-Bootstrap fehlt"

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve"
# Listen: +map/+exec nur nach stuffcmds aus einem .rc (BASEDIR, nicht nur Steam-RODIR).
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cp -a "${GAMEDLL}" "${RUN}/cstrike/dlls/cs_amd64.so"
if [[ -f "${CLIENT}" ]]; then
    cp -a "${CLIENT}" "${RUN}/cstrike/cl_dlls/client_amd64.so"
fi
ln -sfn "${ENG}/engine/libxash.so" "${RUN}/libxash.so"
ln -sfn "${ENG}/ref/gl/libref_gl.so" "${RUN}/libref_gl.so"
ln -sfn "${ENG}/3rdparty/mainui/libmenu.so" "${RUN}/libmenu.so"
ln -sfn "${ENG}/filesystem/filesystem_stdio.so" "${RUN}/filesystem_stdio.so"
ln -sfn "${ENG}/game_launch/xash3d" "${RUN}/xash3d"

printf 'sv_lan 1\nmap %s\n' "${MAP}" > "${RUN}/cstrike/smoke.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/3rdparty/mainui:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
unset STEAM_RUNTIME STEAM_COMPAT_DATA_PATH 2>/dev/null || true

LOG="${RUN}/engine.log"
rm -f "${LOG}"

ARGS=(
    -game cstrike
    -dll "${GAMEDLL}"
    -dev 2
    -log
    +maxplayers 10
    +sv_lan 1
    +exec smoke.cfg
    +map "${MAP}"
)

case "${MODE}" in
    dedicated)
        ARGS=(-dedicated "${ARGS[@]}")
        ;;
    listen)
        [[ -f "${CLIENT}" ]] || fail "Client fehlt für Listen-Modus: ${CLIENT}"
        # Kleines Fenster: Vollbild auf dem Host-Display schließt den Smoke oft mit SDL_QUIT.
        ARGS=(-clientlib "${CLIENT}" -windowed -width 640 -height 480 "${ARGS[@]}")
        ;;
    *)
        fail "Modus: dedicated|listen (got: ${MODE})"
        ;;
esac

cd "${RUN}"
set +e
timeout "${TIMEOUT_SEC}" ./xash3d "${ARGS[@]}" >/dev/null 2>&1
rc=$?
set -e

[[ -f "${LOG}" ]] || fail "kein engine.log nach dem Lauf (exit ${rc})"

if rg -q 'Host_Error|fatal error|Segmentation fault|SIGSEGV' "${LOG}"; then
    rg -n 'Host_Error|fatal error|Segmentation fault|SIGSEGV|can.t initialize' "${LOG}" || true
    fail "Engine-Fehler im Log (exit ${rc})"
fi

if rg -q "can't initialize .*cs_amd64|missing GetEntityAPI|missing GiveFnptrsToDll" "${LOG}"; then
    rg -n "can't initialize|GetEntityAPI|GiveFnptrsToDll" "${LOG}" || true
    fail "GameDLL wurde nicht geladen"
fi

if ! rg -q 'initai?lized (legacy|extended) EntityAPI|Spawn Server:' "${LOG}"; then
    echo "--- engine.log (tail) ---" >&2
    tail -n 40 "${LOG}" >&2
    fail "kein EntityAPI-/Spawn-Server-Nachweis"
fi

if ! rg -q "Spawn Server: ${MAP}|Loading map \"${MAP}\"" "${LOG}"; then
    echo "--- engine.log (tail) ---" >&2
    tail -n 40 "${LOG}" >&2
    fail "Map ${MAP} nicht gestartet"
fi

# timeout 124 = Prozess noch aktiv nach Map-Start — das ist Erfolg.
if [[ "${rc}" -ne 0 && "${rc}" -ne 124 ]]; then
    fail "xash3d exit ${rc} nach erfolgreichem Log-Muster (unerwartet)"
fi

ok "GameDLL geladen, Map ${MAP} gestartet (${MODE}, xash exit ${rc})"
