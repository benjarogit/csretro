#!/usr/bin/env bash
# Desktop-Menüs ohne Auto-Join und ohne touch/*.cfg.
# RODIR = CS-Retro-Game-Data. Kein Steam-HL.
# Headless (gamescope). CSRETRO_FOREGROUND=1 = sichtbares Fenster.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "${ROOT}/scripts/env.sh" ]]; then
    # shellcheck source=env.sh
    source "${ROOT}/scripts/env.sh"
fi

MAP="${CSRETRO_SMOKE_MAP:-de_dust}"
TIMEOUT_SEC="${CSRETRO_MENU_TIMEOUT:-90}"
RUN="${CSRETRO_RUN_DIR:-${ROOT}/build/run-menus}"
ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
if [[ -f "${ROOT}/scripts/gamedata-env.sh" ]]; then
    # shellcheck source=gamedata-env.sh
    source "${ROOT}/scripts/gamedata-env.sh"
fi
# shellcheck source=headless-x11.sh
source "${ROOT}/scripts/headless-x11.sh"
LOG="${RUN}/engine.log"

fail() {
    echo "MENU FAIL: $*" >&2
    exit 1
}

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt."
[[ -f "${GAMEDLL}" ]] || fail "GameDLL fehlt."
[[ -f "${CLIENT}" ]] || fail "Client fehlt."
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data-Bootstrap fehlt"
command -v xdotool >/dev/null 2>&1 || fail "xdotool fehlt"
command -v timeout >/dev/null 2>&1 || fail "timeout fehlt"
if [[ "${CSRETRO_FOREGROUND:-0}" != 1 ]]; then
    command -v gamescope >/dev/null 2>&1 || fail "gamescope fehlt — oder CSRETRO_FOREGROUND=1"
fi

rg -q '^game "CS Retro"' "${GAMEDATA}/cstrike/liblist.gam" || fail "liblist.gam ist nicht CS-Retro-owned"
rg -q 'gamedll_linux "dlls/cs.so"' "${GAMEDATA}/cstrike/liblist.gam" || fail "liblist.gam hat keinen Xash-cs.so-Pfad"

mkdir -p "${RUN}/cstrike/dlls" "${RUN}/cstrike/cl_dlls" "${RUN}/valve"
cp -a "${GAMEDLL}" "${RUN}/cstrike/dlls/cs_amd64.so"
cp -a "${CLIENT}" "${RUN}/cstrike/cl_dlls/client_amd64.so"
ln -sfn "${ENG}/engine/libxash.so" "${RUN}/libxash.so"
ln -sfn "${ENG}/ref/gl/libref_gl.so" "${RUN}/libref_gl.so"
if [[ -f "${MENU}" ]]; then
    cp -a "${MENU}" "${RUN}/menu_amd64.so"
    ln -sfn "${MENU}" "${RUN}/libmenu.so"
else
    ln -sfn "${ENG}/3rdparty/mainui/libmenu.so" "${RUN}/libmenu.so"
fi
ln -sfn "${ENG}/filesystem/filesystem_stdio.so" "${RUN}/filesystem_stdio.so"
ln -sfn "${ENG}/game_launch/xash3d" "${RUN}/xash3d"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"

cat > "${RUN}/cstrike/game_init.cfg" <<'EOF'
bot_enable 1
EOF

cat > "${RUN}/cstrike/listenserver.cfg" <<EOF
log on
mp_logmessages 1
mp_auto_join_team 0
mp_freezetime 15
mp_buytime 2
mp_limitteams 0
mp_autoteambalance 0
mp_roundtime 5
sv_cheats 1
sv_lan 1
bot_enable 1
bot_join_team T
bot_quota 1
bot_difficulty 0
EOF

cat > "${RUN}/cstrike/userconfig.cfg" <<'EOF'
_vgui_menus 1
bind "1" "slot1"
bind "2" "slot2"
bind "5" "slot5"
bind "F6" "exec menus-flow.cfg"
developer 2
echo CSRETRO_MENU_CFG_LOADED
EOF
cat > "${RUN}/cstrike/menus-flow.cfg" <<'EOF'
jointeam 2
echo CSRETRO_MENU_TEAM_CT
wait 250
joinclass 5
echo CSRETRO_MENU_CLASS_AUTO
wait 450
buy
echo CSRETRO_MENU_BUYCMD
wait 120
menuselect 1
echo CSRETRO_MENU_BUY_CAT
wait 90
glock
echo CSRETRO_MENU_BUY_GUN
wait 90
radio1
echo CSRETRO_MENU_RADIO1
wait 90
menuselect 1
echo CSRETRO_MENU_RADIO_PICK
wait 60
+forward
echo CSRETRO_MENU_MOVE
-forward
+attack
echo CSRETRO_MENU_ATTACK
-attack
wait 80
quit
echo CSRETRO_MENU_QUIT
EOF
printf '%s\n' 'exec userconfig.cfg' > "${RUN}/cstrike/config.cfg"
cp -a "${RUN}/cstrike/userconfig.cfg" "${RUN}/cstrike/autoexec.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/3rdparty/mainui:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
unset STEAM_RUNTIME STEAM_COMPAT_DATA_PATH 2>/dev/null || true
CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope.log"
csretro_headless_x11_prepare || fail "gamescope fehlt — oder CSRETRO_FOREGROUND=1 für sichtbares Fenster"

rm -f "${LOG}"
killall -q xash3d 2>/dev/null || true
sleep 0.4

MENU_ARGS=()
if [[ -f "${MENU}" ]]; then
    MENU_ARGS=(-menulib "$(readlink -f "${MENU}")")
fi

cd "${RUN}"
set +e
csretro_headless_x11_wrap ./xash3d \
    -game cstrike \
    -dll "${GAMEDLL}" \
    -clientlib "${CLIENT}" \
    "${MENU_ARGS[@]}" \
    -windowed -width 640 -height 480 \
    -dev 2 \
    -log \
    +maxplayers 10 \
    +sv_lan 1 \
    +map "${MAP}" \
    +exec userconfig.cfg \
    >/dev/null 2>>"${CSRETRO_GAMESCOPE_LOG}" &
XASH_PID=$!
CSRETRO_GAMESCOPE_PID="${XASH_PID}"
set -e
csretro_headless_x11_wait_display || fail "headless-X11 nicht bereit"

(
    sleep "${TIMEOUT_SEC}"
    if kill -0 "${XASH_PID}" >/dev/null 2>&1; then
        kill -TERM -- -"${XASH_PID}" >/dev/null 2>&1 || kill -TERM "${XASH_PID}" >/dev/null 2>&1 || true
    fi
) &
WATCHDOG_PID=$!

cleanup() {
    kill "${WATCHDOG_PID}" >/dev/null 2>&1 || true
    csretro_headless_x11_stop
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

wait_log "Spawn Server: ${MAP}|Loading map \"${MAP}\"" 40 || fail "Map ${MAP} nicht gestartet"
wait_log 'CSRetro-VGUI:|CSRetro-Menu:' 20 || fail "kein Team-Menü (VGUI oder ShowMenu)"
sleep 1
WID=""
for _ in $(seq 1 20); do
    WID="$(xdotool search --name 'CS Retro' 2>/dev/null | head -n1 || true)"
    if [[ -z "${WID}" ]]; then
        WID="$(xdotool search --name 'Counter-Strike' 2>/dev/null | head -n1 || true)"
    fi
    if [[ -z "${WID}" ]]; then
        WID="$(xdotool search --class 'xash' 2>/dev/null | head -n1 || true)"
    fi
    if [[ -n "${WID}" ]]; then
        break
    fi
    sleep 0.25
done
[[ -n "${WID}" ]] || fail "kein Fenster auf DISPLAY=${DISPLAY:-?} (headless Xwayland?)"
xdotool key --window "${WID}" F6 >/dev/null 2>&1 || true

wait_log 'joined team' 25 || fail "manuelle Team-Auswahl fehlgeschlagen"
xdotool key --window "${WID}" F6 >/dev/null 2>&1 || true
wait_log 'CSRETRO_MENU_BUYCMD|CSRetro-Menu: #Buy' 20 || true
wait_log 'CSRETRO_MENU_RADIO1|CSRetro-Menu: #RadioA' 15 || true
wait_log 'Issuing host shutdown|Server shutdown|CSRETRO_MENU_QUIT' 25 || true

wait_log 'Issuing host shutdown|Server shutdown' 12 || true
if kill -0 "${XASH_PID}" >/dev/null 2>&1; then
    kill -TERM -- -"${XASH_PID}" >/dev/null 2>&1 || kill -TERM "${XASH_PID}" >/dev/null 2>&1 || true
    sleep 1
fi
wait "${XASH_PID}" >/dev/null 2>&1 || true
trap - EXIT

[[ -f "${LOG}" ]] || fail "kein engine.log"
if rg -q 'steamapps/common/Half-Life' "${LOG}"; then
    fail "engine.log enthält Steam-Half-Life-Pfade"
fi
if rg -q 'MenuFactory is unavailable' "${LOG}"; then
    fail "MenuFactory-Dialog-Text im Log"
fi
if rg -q 'exec touch/' "${LOG}"; then
    fail "Touch-CFG-Pfad wurde ausgeführt"
fi
if rg -q 'Host_Error|fatal error|Segmentation fault|SIGSEGV' "${LOG}"; then
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

echo "=== Desktop-Menüs ==="
failed=0
check "rodir" "Dokumente/csretro/gamedata/|Adding directory: .*/gamedata" || failed=1
check "no_dialog" "CSRetro-VGUI:|CSRetro-Menu:" || failed=1
check "team_menu" 'CSRetro-VGUI:.*Teammenu|CSRetro-Menu: #Team_Select|CSRetro-Menu: #IG_Team_Select|CSRetro-Menu: #Team_Select_Spect' || failed=1
check "manual_team" 'joined team' || failed=1
check "class_or_spawn" 'CSRetro-VGUI:.*Classmenu|CSRetro-Menu: #CT_Select|CSRetro-Menu: #Terrorist_Select|game_playerspawn' || failed=1
check "buy" 'CSRETRO_MENU_BUYCMD|CSRetro-VGUI:.*Buy|CSRetro-Menu: #Buy|CSRetro-Menu: #T_Buy|CSRetro-Menu: #CT_Buy' || failed=1
check "radio" 'CSRETRO_MENU_RADIO1|CSRetro-Menu: #RadioA|CSRetro-VGUI:.*Radio' || failed=1
check "move" 'CSRETRO_MENU_MOVE' || failed=1
check "attack" 'CSRETRO_MENU_ATTACK' || failed=1
check "shutdown" 'Issuing host shutdown|Server shutdown|CSRETRO_MENU_QUIT' || failed=1

if [[ "${failed}" -ne 0 ]]; then
    echo "--- engine.log (Menü) ---" >&2
    rg -n 'CSRetro-VGUI|CSRetro-Menu|CSRETRO_MENU_|joined team|touch/|MenuFactory|shutdown' "${LOG}" >&2 || true
    tail -n 30 "${LOG}" >&2
    fail "mindestens ein Menü-Marker fehlt"
fi

echo "MENU OK: Desktop-Team/Buy/Radio ohne touch/*.cfg"
exit 0
