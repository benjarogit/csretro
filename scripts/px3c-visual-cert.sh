#!/usr/bin/env bash
# PX3C visual cert: r_csretro_renderer 1, visible frame stays Xash.
# Shots: build/px3c-cert-shots/ (not committed).
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
MAP="de_dust"
OUT="${ROOT}/build/px3c-cert-shots"

fail() { echo "PX3C_VIS FAIL: $*" >&2; exit 1; }

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" && -f "${GAMEDLL}" && -f "${MENU}" ]] || fail "Binaries fehlen"
command -v xdotool >/dev/null || fail "xdotool fehlt"
command -v import >/dev/null || fail "import fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

export CSRETRO_ENGINE_OUT="$(readlink -f "${ENG}")"
export CSRETRO_CLIENT_SO="$(readlink -f "${CLIENT}")"
export CSRETRO_GAMEDLL_SO="$(readlink -f "${GAMEDLL}")"
export CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
csretro_gate_isolate_begin "px3c-vis" || fail "isolate"
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
mp_auto_join_team 0
mp_freezetime 0
mp_round_infinite 1
mp_limitteams 0
mp_autoteambalance 0
bot_quota 0
sv_lan 1
_vgui_menus 1
bind "F5" "map de_dust"
bind "F6" "map de_aztec"
bind "F7" "vid_setmode 1024 768"
bind "F8" "quit"
bind "F9" "chooseteam"
bind "F10" "buy"
bind "F2" "exec px3c-he.cfg"
bind "F3" "exec px3c-smoke.cfg"
bind "F4" "exec px3c-flash.cfg"
bind "ESCAPE" "cancelselect"
echo CSRETRO_PX3C_VIS_CFG
EOF
cat > "${RUN}/cstrike/px3c-he.cfg" <<'EOF'
give weapon_hegrenade
echo CSRETRO_PX3C_GIVE_HE
+attack
wait 40
-attack
echo CSRETRO_PX3C_THROW_HE
EOF
cat > "${RUN}/cstrike/px3c-smoke.cfg" <<'EOF'
give weapon_smokegrenade
echo CSRETRO_PX3C_GIVE_SMOKE
+attack
wait 40
-attack
echo CSRETRO_PX3C_THROW_SMOKE
EOF
cat > "${RUN}/cstrike/px3c-flash.cfg" <<'EOF'
give weapon_flashbang
echo CSRETRO_PX3C_GIVE_FLASH
+attack
wait 40
-attack
echo CSRETRO_PX3C_THROW_FLASH
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_W=1280
export CSRETRO_GAMESCOPE_H=720
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px3c-vis.log"

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
	+maxplayers 4 \
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

sleep 4
shot 01-team-t-open
press 1
sleep 2
shot 02-class-t
press 1
sleep 3
shot 03-spawn-t-viewmodel-hud
press F10
sleep 2
shot 04-buy-t
press Escape
sleep 1

press F2
sleep 1
shot 05-he-deploy
sleep 2
shot 06-he-after-throw
press F3
sleep 1
shot 07-smoke-deploy
sleep 3
shot 08-smoke-after-throw
press F4
sleep 1
shot 09-flash-deploy
sleep 2
shot 10-flash-after-throw

# CT via chooseteam
press F9
sleep 2
shot 11-team-ct-open
press 2
sleep 2
shot 12-class-ct
press 1
sleep 3
shot 13-spawn-ct-viewmodel-hud
press F10
sleep 2
shot 14-buy-ct
press Escape
sleep 1

press F6
sleep 6
shot 15-mapchange-aztec
press F7
sleep 2
shot 16-vid-setmode
press F8
csretro_gate_wait_quit "${XASH_PID}" 15 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px3c-vis.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi

mkdir -p "${OUT}"
cp -a "${SHOTS}/." "${OUT}/"
echo "PX3C_VIS PASS"
echo "PX3C_VIS shots=${OUT}"
echo "PX3C_VIS log=${ALL}"
ls -1 "${OUT}"
exit 0
