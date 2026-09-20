#!/usr/bin/env bash
# #7 Spectator Map-Overview runtime cert. Visible frame stays Xash (GL_RenderFrame = 0).
# Enters real OBS_MAP_FREE / OBS_MAP_CHASE, not FPS spectator.
# Shots: build/px7-tri-overview-cert-shots/ (not committed).
# Usage: ./scripts/build-client.sh && ./scripts/px7-tri-overview-cert.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "${ROOT}/scripts/gamedata-env.sh" ]] && source "${ROOT}/scripts/gamedata-env.sh"
source "${ROOT}/scripts/headless-x11.sh"
source "${ROOT}/scripts/gate-crash.sh"
source "${ROOT}/scripts/gate-isolate.sh"

ENG="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
CLIENT="${CSRETRO_CLIENT_SO:-${ROOT}/build/client-cmake/client/client_amd64.so}"
GAMEDLL="${CSRETRO_GAMEDLL_SO:-${ROOT}/build/gamedll-cmake/cs_amd64.so}"
MENU="${CSRETRO_MENU_SO:-${ROOT}/build/client-cmake/menu/menu_amd64.so}"
MAP="de_aztec"
OUT="${ROOT}/build/px7-tri-overview-cert-shots"

fail() { echo "PX7_TRI_OVERVIEW FAIL: $*" >&2; exit 1; }

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" && -f "${GAMEDLL}" && -f "${MENU}" ]] || fail "Binaries fehlen"
command -v xdotool >/dev/null || fail "xdotool fehlt"
command -v import >/dev/null || fail "import fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
[[ -f "${GAMEDATA}/cstrike/overviews/de_aztec.txt" ]] || fail "de_aztec Overview-Datei fehlt"
[[ -f "${GAMEDATA}/cstrike/overviews/de_aztec.bmp" ]] || fail "de_aztec Overview-Layer fehlt"

export CSRETRO_ENGINE_OUT="$(readlink -f "${ENG}")"
export CSRETRO_CLIENT_SO="$(readlink -f "${CLIENT}")"
export CSRETRO_GAMEDLL_SO="$(readlink -f "${GAMEDLL}")"
export CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
csretro_gate_isolate_begin "px7-tri-ov" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${RUN}/shots"
mkdir -p "${SHOTS}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
r_csretro_renderer 1
sv_cheats 1
allow_spectators 1
mp_forcecamera 0
mp_fadetoblack 0
mp_auto_join_team 0
mp_freezetime 12
mp_round_infinite 1
mp_limitteams 0
mp_autoteambalance 0
bot_quota 2
bot_join_after_player 0
bot_stop 1
sv_lan 1
_vgui_menus 1
bind "SPACE" "+jump"
bind "c" "+duck"
bind "F1" "exec px7-ov-join.spec.cfg"
bind "F2" "exec px7-ov-mapfree.cfg"
bind "F3" "exec px7-ov-mapchase.cfg"
bind "F4" "exec px7-ov-ineye.cfg"
bind "F8" "quit"
bind "F9" "chooseteam"
bind "ESCAPE" "cancelselect"
echo CSRETRO_PX7_TRI_OVERVIEW_CFG
EOF
cat > "${RUN}/cstrike/px7-ov-join.spec.cfg" <<'EOF'
sv_cheats 1
allow_spectators 1
jointeam 6
echo CSRETRO_PX7_OV_JOIN_SPEC
EOF
cat > "${RUN}/cstrike/px7-ov-mapfree.cfg" <<'EOF'
spec_mode 5
cmd specmode 5
echo CSRETRO_PX7_OV_MAP_FREE
EOF
cat > "${RUN}/cstrike/px7-ov-mapchase.cfg" <<'EOF'
spec_mode 6
cmd specmode 6
echo CSRETRO_PX7_OV_MAP_CHASE
EOF
cat > "${RUN}/cstrike/px7-ov-ineye.cfg" <<'EOF'
spec_mode 4
cmd specmode 4
echo CSRETRO_PX7_OV_IN_EYE
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_W=1280
export CSRETRO_GAMESCOPE_H=720
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px7-tri-ov.log"

csretro_headless_x11_prepare || fail "gamescope/display"
rm -f "${LOG}"
: >"${CSRETRO_GAMESCOPE_LOG}"

cd "${RUN}"
set +e
csretro_headless_x11_wrap ./xash3d \
	-game cstrike \
	-dll "${GAMEDLL}" \
	-clientlib "${CLIENT}" \
	-menulib "${MENU}" \
	-windowed -width 1280 -height 720 \
	-dev 2 -log \
	+maxplayers 8 \
	+sv_lan 1 \
	+map "${MAP}" \
	+exec autoexec.cfg \
	>>"${CSRETRO_GAMESCOPE_LOG}" 2>&1 &
XASH_PID=$!
CSRETRO_GAMESCOPE_PID="${XASH_PID}"
set -e
cd "${ROOT}"

csretro_headless_x11_wait_display || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	fail "X11"
}

WID=""
for _ in $(seq 1 80); do
	WID="$(xdotool search --name 'CS Retro' 2>/dev/null | head -n1 || true)"
	[[ -z "${WID}" ]] && WID="$(xdotool search --name 'Counter-Strike' 2>/dev/null | head -n1 || true)"
	[[ -n "${WID}" ]] && break
	sleep 0.25
done
[[ -n "${WID}" ]] || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein Fenster"
}

shot() { import -window "${WID}" "${SHOTS}/$1.png"; }
press() { xdotool key --window "${WID}" "$1" >/dev/null 2>&1 || true; }
tap_jump() {
	xdotool keydown --window "${WID}" space >/dev/null 2>&1 || true
	sleep 0.12
	xdotool keyup --window "${WID}" space >/dev/null 2>&1 || true
	sleep 0.45
}

sleep 4
shot 01-aztec-team-open
press 1
sleep 2
shot 02-aztec-class-t
press 1
sleep 3
shot 03-aztec-spawn-fps
sleep 1
press F1
sleep 3
shot 04-spectator-joined
# Server-Observer: JUMP wechselt CHASE_FREE → IN_EYE → ROAMING → MAP_FREE.
# spec_mode/specmode bleibt zusätzlich, JUMP ist der Stock-Pfad.
press F2
sleep 1
tap_jump
tap_jump
tap_jump
sleep 2
shot 05-overview-map-free
sleep 1
shot 06-overview-map-free-follow
press F3
sleep 1
tap_jump
sleep 2
shot 07-overview-map-chase
sleep 1
shot 08-overview-map-chase-follow
press F4
sleep 1
tap_jump
sleep 2
shot 09-spectator-left-overview
sleep 1
shot 10-spectator-left-overview-follow
press F8
csretro_gate_wait_quit "${XASH_PID}" 15 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px7-tri-ov.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi
rg -q 'CSRETRO_PX7_OV_JOIN_SPEC' "${ALL}" || fail "jointeam Spectator nicht geloggt"
rg -q 'CSRETRO_PX7_OV_MAP_FREE' "${ALL}" || fail "spec_mode MAP_FREE nicht geloggt"
rg -q 'CSRETRO_PX7_OV_MAP_CHASE' "${ALL}" || fail "spec_mode MAP_CHASE nicht geloggt"
rg -q 'CSRETRO_PX7_OV_IN_EYE' "${ALL}" || fail "spec_mode IN_EYE (Overview verlassen) nicht geloggt"
if ! rg -q 'tri overview draw-only reached user1=5|tri overview draw-only reached user1=6' "${ALL}"; then
	fail "kein echter Map-Overview (OBS_MAP_FREE/CHASE) im draw-only Pfad"
fi
if rg -q 'tri overview list before=.*mutate=1' "${ALL}"; then
	fail "draw-only hat Overview-Liste mutiert"
fi
if rg -q 'tri overview gl_clear before=.*mutate=1' "${ALL}"; then
	fail "draw-only hat gl_clear mutiert"
fi
if rg -q 'tri overview state before=.*mutate=1' "${ALL}"; then
	fail "draw-only hat Overview-State mutiert"
fi
rg -q 'tri overview gl_clear restore' "${ALL}" || fail "gl_clear Restore beim Verlassen fehlt"
if rg -q 'tri xash export extra' "${ALL}"; then
	fail "doppelter Triangle-Advance"
fi

mkdir -p "${OUT}"
cp -a "${SHOTS}/." "${OUT}/"
echo "PX7_TRI_OVERVIEW PASS"
echo "PX7_TRI_OVERVIEW shots=${OUT}"
echo "PX7_TRI_OVERVIEW log=${ALL}"
ls -1 "${OUT}"
rg -n 'tri overview|offscreen tri proof .*pass=normal|gl_clear restore|PX7_OV_|visible frame stays Xash|GL_RenderFrame' "${ALL}" | head -n 80
exit 0
