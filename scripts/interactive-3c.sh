#!/usr/bin/env bash
# Interaktiver 3C-Durchlauf: Team/Spawn/Movement/Waffen/Round/Shutdown.
# Runtime-Daten: XASH3D_RODIR = CS-Retro-Game-Data (nicht Steam-HL).
# Erfolg/Fehler über Exitcode und Checkliste.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "${ROOT}/scripts/env.sh" ]]; then
    # shellcheck source=env.sh
    source "${ROOT}/scripts/env.sh"
fi

MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
TIMEOUT_SEC="${CSRETRO_3C_TIMEOUT:-90}"
RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run}"
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
if [[ -f "${ROOT}/scripts/gamedata-env.sh" ]]; then
    # shellcheck source=gamedata-env.sh
    source "${ROOT}/scripts/gamedata-env.sh"
fi
LOG="${RUN}/engine.log"

fail() {
    echo "3C FAIL: $*" >&2
    exit 1
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "$1 fehlt"
}

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt. ./scripts/build-engine.sh"
[[ -f "${GAMEDLL}" ]] || fail "GameDLL fehlt. ./scripts/build-gamedll.sh"
[[ -f "${CLIENT}" ]] || fail "Client fehlt. ./scripts/build-client.sh"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data-Bootstrap fehlt"
need_cmd timeout

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/cstrike/maps" "${RUN}/valve" /tmp/csretro-zbot
cp -a "${GAMEDLL}" "${RUN}/cstrike/dlls/cs_amd64.so"
cp -a "${CLIENT}" "${RUN}/cstrike/cl_dlls/client_amd64.so"
ln -sfn "${ENG}/engine/libxash.so" "${RUN}/libxash.so"
ln -sfn "${ENG}/ref/gl/libref_gl.so" "${RUN}/libref_gl.so"
ln -sfn "${ENG}/3rdparty/mainui/libmenu.so" "${RUN}/libmenu.so"
ln -sfn "${ENG}/filesystem/filesystem_stdio.so" "${RUN}/filesystem_stdio.so"
ln -sfn "${ENG}/game_launch/xash3d" "${RUN}/xash3d"

# ZBot-Testdaten kommen aus dem Game-Data-Baum (nicht Steam). BASEDIR nur falls noch lokal.
if [[ ! -f "${RUN}/cstrike/BotProfile.db" && -f "${GAMEDATA}/cstrike/BotProfile.db" ]]; then
    cp -a "${GAMEDATA}/cstrike/BotProfile.db" "${RUN}/cstrike/BotProfile.db"
    cp -a "${GAMEDATA}/cstrike/BotChatter.db" "${RUN}/cstrike/BotChatter.db"
fi
if [[ ! -s "${RUN}/cstrike/maps/${MAP}.nav" && -s "${GAMEDATA}/cstrike/maps/${MAP}.nav" ]]; then
    mkdir -p "${RUN}/cstrike/maps"
    cp -a "${GAMEDATA}/cstrike/maps/${MAP}.nav" "${RUN}/cstrike/maps/${MAP}.nav"
fi
[[ -f "${GAMEDATA}/cstrike/BotProfile.db" || -f "${RUN}/cstrike/BotProfile.db" ]] \
    || fail "BotProfile.db fehlt im Game-Data-Baum. python3 ./scripts/bootstrap-gamedata.py"

# Listen führt +Befehle nur aus, wenn ein .rc `stuffcmds` enthält.
# Steam-valve.rc liegt in RODIR und wird von FileExists in BASEDIR oft nicht gesehen.
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"

cat > "${RUN}/cstrike/game_init.cfg" <<'EOF'
bot_enable 1
EOF

cat > "${RUN}/cstrike/listenserver.cfg" <<EOF
log on
mp_logmessages 1
mp_auto_join_team 1
humans_join_team CT
mp_freezetime 0
mp_limitteams 0
mp_autoteambalance 0
mp_roundtime 5
sv_cheats 1
sv_lan 1
bot_enable 1
bot_join_team T
bot_quota 1
bot_difficulty 0
cl_showerror 1
EOF

cat > "${RUN}/cstrike/userconfig.cfg" <<'EOF'
alias +csretro_fwd "+forward; echo CSRETRO_3C_MOVE_FORWARD"
alias -csretro_fwd "-forward"
alias +csretro_jump "+jump; echo CSRETRO_3C_JUMP"
alias -csretro_jump "-jump"
alias +csretro_duck "+duck; echo CSRETRO_3C_DUCK"
alias -csretro_duck "-duck"
alias +csretro_atk "+attack; echo CSRETRO_3C_ATTACK"
alias -csretro_atk "-attack"
alias +csretro_rel "+reload; echo CSRETRO_3C_RELOAD"
alias -csretro_rel "-reload"
bind "w" "+csretro_fwd"
bind "SPACE" "+csretro_jump"
bind "CTRL" "+csretro_duck"
bind "MOUSE1" "+csretro_atk"
bind "F9" "+csretro_atk"
bind "r" "+csretro_rel"
bind "1" "slot1; echo CSRETRO_3C_SLOT1"
bind "2" "slot2; echo CSRETRO_3C_SLOT2"
bind "3" "slot3; echo CSRETRO_3C_SLOT3"
bind "F6" "exec 3c-actions.cfg"
bind "F7" "bot_add_t; echo CSRETRO_3C_BOT_ADD_T"
bind "F8" "quit; echo CSRETRO_3C_QUIT"
developer 2
cl_showerror 1
echo CSRETRO_3C_CFG_LOADED
EOF
cat > "${RUN}/cstrike/3c-actions.cfg" <<'EOF'
give weapon_ak47
echo CSRETRO_3C_GIVE_AK47
bot_add_t
echo CSRETRO_3C_BOT_ADD_T
+forward
echo CSRETRO_3C_MOVE_FORWARD
+jump
echo CSRETRO_3C_JUMP
+duck
echo CSRETRO_3C_DUCK
slot3
echo CSRETRO_3C_SLOT3
slot2
echo CSRETRO_3C_SLOT2
slot1
echo CSRETRO_3C_SLOT1
+attack
echo CSRETRO_3C_ATTACK
+reload
echo CSRETRO_3C_RELOAD
EOF
# wait N = N gerenderte Frames (144 Hz-Host ≈ 6 s bei 900).
cat > "${RUN}/cstrike/3c-delay.cfg" <<'EOF'
wait 4000
exec 3c-actions.cfg
wait 500
quit
EOF
cp -a "${RUN}/cstrike/userconfig.cfg" "${RUN}/cstrike/autoexec.cfg"
if [[ -f "${RUN}/cstrike/config.cfg" ]] && ! rg -q '^exec userconfig\.cfg' "${RUN}/cstrike/config.cfg"; then
    printf '\nexec userconfig.cfg\n' >> "${RUN}/cstrike/config.cfg"
fi

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/3rdparty/mainui:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
unset STEAM_RUNTIME STEAM_COMPAT_DATA_PATH 2>/dev/null || true
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
export DISPLAY="${DISPLAY:-:0}"

rm -f "${LOG}"
if pgrep -f "${RUN}/xash3d" >/dev/null 2>&1; then
    pkill -f "${RUN}/xash3d" >/dev/null 2>&1 || true
    sleep 0.4
fi

cd "${RUN}"
set +e
setsid ./xash3d \
    -game cstrike \
    -dll "${GAMEDLL}" \
    -clientlib "${CLIENT}" \
    -windowed -width 640 -height 480 \
    -dev 2 \
    -log \
    +maxplayers 10 \
    +sv_lan 1 \
    +map "${MAP}" \
    +exec userconfig.cfg \
    >/dev/null 2>&1 &
XASH_PID=$!
set -e

(
    sleep "${TIMEOUT_SEC}"
    if kill -0 "${XASH_PID}" >/dev/null 2>&1; then
        kill -TERM -- -"${XASH_PID}" >/dev/null 2>&1 || kill -TERM "${XASH_PID}" >/dev/null 2>&1 || true
    fi
) &
WATCHDOG_PID=$!

cleanup() {
    kill "${WATCHDOG_PID}" >/dev/null 2>&1 || true
    if kill -0 "${XASH_PID}" >/dev/null 2>&1; then
        kill -TERM -- -"${XASH_PID}" >/dev/null 2>&1 || kill -TERM "${XASH_PID}" >/dev/null 2>&1 || true
        sleep 1
        kill -KILL -- -"${XASH_PID}" >/dev/null 2>&1 || kill -KILL "${XASH_PID}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

wait_log() {
    local pattern="$1"
    local seconds="$2"
    local i=0
    while (( i < seconds )); do
        if [[ -f "${LOG}" ]] && rg -q -- "${pattern}" "${LOG}"; then
            return 0
        fi
        if ! kill -0 "${XASH_PID}" >/dev/null 2>&1; then
            return 1
        fi
        sleep 1
        i=$((i + 1))
    done
    return 1
}

wait_log "Spawn Server: ${MAP}|Loading map \"${MAP}\"" 40 || fail "Map ${MAP} nicht gestartet (stuffcmds/cstrike.rc?)"
wait_log 'joined team' 25 || true
sleep 2

WID="$(xdotool search --onlyvisible --name 'Counter-Strike' 2>/dev/null | head -n1 || true)"
if [[ -n "${WID}" ]]; then
    xdotool key --window "${WID}" F6 >/dev/null 2>&1 || true
    sleep 2
    xdotool key --window "${WID}" F8 >/dev/null 2>&1 || true
    sleep 2
fi

wait_log 'Issuing host shutdown|Server shutdown' 15 || true
if kill -0 "${XASH_PID}" >/dev/null 2>&1; then
    kill -TERM -- -"${XASH_PID}" >/dev/null 2>&1 || kill -TERM "${XASH_PID}" >/dev/null 2>&1 || true
    sleep 1
fi
wait "${XASH_PID}" >/dev/null 2>&1 || true
trap - EXIT

[[ -f "${LOG}" ]] || fail "kein engine.log"

if rg -q 'steamapps/common/Half-Life' "${LOG}"; then
    fail "engine.log enthält noch Steam-Half-Life-Pfade — RODIR darf nicht Steam sein"
fi

if rg -q 'Host_Error|fatal error|Segmentation fault|SIGSEGV' "${LOG}"; then
    rg -n 'Host_Error|fatal error|Segmentation fault|SIGSEGV' "${LOG}" || true
    fail "Engine-Fehler im Log"
fi

check() {
    local id="$1"
    local pattern="$2"
    if rg -q -- "${pattern}" "${LOG}"; then
        echo "  PASS  ${id}"
        return 0
    fi
    echo "  FAIL  ${id}  (Muster: ${pattern})"
    return 1
}

echo "=== 3C interaktiv ==="
failed=0
check "rodir" "Dokumente/csretro/gamedata/|Adding directory: .*/gamedata" || failed=1
check "map" "Spawn Server: ${MAP}|Loading map \"${MAP}\"" || failed=1
check "gamedll" "initailized legacy EntityAPI|initailized extended EntityAPI|ReGameDLL version" || failed=1
check "connect" "client connected" || failed=1
check "team" 'joined team' || failed=1
check "spawn" 'Firing: \(game_playerspawn\)|game_playerspawn' || failed=1
check "hud" 'CS16Client|hud\.txt|CL_LoadProgs: found single callback export' || failed=1
check "move" 'CSRETRO_3C_MOVE_FORWARD' || failed=1
check "jump" 'CSRETRO_3C_JUMP' || failed=1
check "duck" 'CSRETRO_3C_DUCK' || failed=1
check "slot" 'CSRETRO_3C_SLOT' || failed=1
check "attack" 'CSRETRO_3C_ATTACK' || failed=1
check "reload" 'CSRETRO_3C_RELOAD' || failed=1
check "give_or_cheats" 'CSRETRO_3C_GIVE_AK47|give weapon_ak47' || failed=1
check "bot_or_round" 'bot_add|Added bot|joined team "TERRORIST"|World triggered "Round_Start"|World triggered "Game_Commencing"' || failed=1
check "shutdown" 'Issuing host shutdown|Server shutdown|CSRETRO_3C_QUIT' || failed=1

if rg -q 'prediction error:' "${LOG}"; then
    echo "  WARN  prediction  (cl_showerror hat prediction error geloggt)"
else
    echo "  PASS  prediction  (kein 'prediction error:' im Log)"
fi

if [[ "${failed}" -ne 0 ]]; then
    echo "--- engine.log (relevante Zeilen) ---" >&2
    rg -n 'CSRETRO_3C_|joined team|game_player|Round_|hud\.txt|Host_Error|shutdown|bot_|Added bot|EntityAPI|client connected' "${LOG}" >&2 || true
    echo "--- tail ---" >&2
    tail -n 40 "${LOG}" >&2
    fail "mindestens ein Pflicht-Marker fehlt"
fi

echo "3C OK: interaktiver Listen-Durchlauf auf ${MAP}"
exit 0
