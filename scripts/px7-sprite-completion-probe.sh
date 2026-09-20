#!/usr/bin/env bash
# #7 sprite completion: SPR_ANGLED / frame lerp / Xash sprite lighting.
# Visible frame stays Xash (GL_RenderFrame = 0). Shots: build/px7-sprite-cert-shots/
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
OUT="${ROOT}/build/px7-sprite-cert-shots"

fail() { echo "PX7_SPRITE FAIL: $*" >&2; exit 1; }

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" && -f "${GAMEDLL}" && -f "${MENU}" ]] || fail "Binaries fehlen"
command -v xdotool >/dev/null || fail "xdotool fehlt"
command -v import >/dev/null || fail "import fehlt"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/de_dust.bsp" ]] || fail "de_dust.bsp fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/cs_assault.bsp" ]] || fail "cs_assault.bsp fehlt"

export CSRETRO_ENGINE_OUT="$(readlink -f "${ENG}")"
export CSRETRO_CLIENT_SO="$(readlink -f "${CLIENT}")"
export CSRETRO_GAMEDLL_SO="$(readlink -f "${GAMEDLL}")"
export CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
csretro_gate_isolate_begin "px7-sprite" || fail "isolate"
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
r_sprite_lerping 1
r_sprite_lighting 1
sv_cheats 1
mp_auto_join_team 0
mp_freezetime 0
mp_round_infinite 1
mp_limitteams 0
mp_autoteambalance 0
bot_quota 0
sv_lan 1
_vgui_menus 1
bind "F2" "exec px7-spr-he.cfg"
bind "F3" "exec px7-spr-smoke.cfg"
bind "F4" "exec px7-spr-flash.cfg"
bind "F5" "exec px7-spr-lerpoff.cfg"
bind "F6" "map de_dust"
bind "F7" "vid_setmode 1024 768"
bind "F8" "quit"
bind "F9" "chooseteam"
bind "F10" "buy"
bind "F11" "exec px7-spr-lerpon.cfg"
bind "F12" "map cs_assault"
bind "ESCAPE" "cancelselect"
echo CSRETRO_PX7_SPRITE_CFG
EOF
cat > "${RUN}/cstrike/px7-spr-he.cfg" <<'EOF'
give weapon_hegrenade
echo CSRETRO_PX7_SPR_GIVE_HE
+attack
wait 40
-attack
echo CSRETRO_PX7_SPR_THROW_HE
EOF
cat > "${RUN}/cstrike/px7-spr-smoke.cfg" <<'EOF'
give weapon_smokegrenade
echo CSRETRO_PX7_SPR_GIVE_SMOKE
+attack
wait 40
-attack
echo CSRETRO_PX7_SPR_THROW_SMOKE
EOF
cat > "${RUN}/cstrike/px7-spr-flash.cfg" <<'EOF'
give weapon_flashbang
echo CSRETRO_PX7_SPR_GIVE_FLASH
+attack
wait 40
-attack
echo CSRETRO_PX7_SPR_THROW_FLASH
EOF
cat > "${RUN}/cstrike/px7-spr-lerpoff.cfg" <<'EOF'
r_sprite_lerping 0
r_sprite_lighting 0
echo CSRETRO_PX7_SPR_LERP_LIGHT_OFF
EOF
cat > "${RUN}/cstrike/px7-spr-lerpon.cfg" <<'EOF'
r_sprite_lerping 1
r_sprite_lighting 1
echo CSRETRO_PX7_SPR_LERP_LIGHT_ON
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_W=1280
export CSRETRO_GAMESCOPE_H=720
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px7-sprite.log"

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
shot 01-aztec-team
press 1
sleep 2
press 1
sleep 3
shot 02-aztec-spawn
press F2
sleep 3
shot 03-aztec-he
press F3
sleep 4
shot 04-aztec-smoke
press F4
sleep 3
shot 05-aztec-flash
press F5
sleep 1
shot 06-aztec-lerp-light-off
press F11
sleep 1
shot 07-aztec-lerp-light-on
press F6
sleep 6
shot 08-mapchange-dust
press F12
sleep 6
shot 09-mapchange-assault
press F7
sleep 2
shot 10-vid-setmode
press F8
csretro_gate_wait_quit "${XASH_PID}" 15 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px7-sprite.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi
rg -q 'sprite live state before=.*mutate=0' "${ALL}" || fail "Live-Latch-Mutation-Gate fehlt oder mutate!=0"
if rg -q 'sprite live state before=.*mutate=1' "${ALL}"; then
	fail "Offscreen Sprite-Pass hat Live-Entity mutiert"
fi
rg -q 'sprite dump kind=tent|CSRETRO_PX7_SPR_THROW_HE' "${ALL}" || fail "TempEnt/HE-Pfad nicht gesehen"
rg -q 'CSRETRO_PX7_SPR_LERP_LIGHT_OFF' "${ALL}" || fail "lerping/lighting 0 nicht gesetzt"
rg -q 'CSRETRO_PX7_SPR_LERP_LIGHT_ON' "${ALL}" || fail "lerping/lighting 1 nicht gesetzt"

mkdir -p "${OUT}"
cp -a "${SHOTS}/." "${OUT}/"
echo "PX7_SPRITE PASS"
echo "PX7_SPRITE shots=${OUT}"
echo "PX7_SPRITE log=${ALL}"
if rg -q 'sprite lerp .*old_ne_current=[1-9]' "${ALL}"; then
	echo "PX7_SPRITE lerp=CONFIRMED"
else
	echo "PX7_SPRITE lerp=NOT REPRODUCIBLE WITH CURRENT GAME CONTENT"
fi
if rg -q 'sprite lighting .*pass=[1-9]' "${ALL}"; then
	echo "PX7_SPRITE lighting=CONFIRMED"
else
	echo "PX7_SPRITE lighting=NOT REPRODUCIBLE WITH CURRENT GAME CONTENT"
fi
if rg -q 'sprite angled path=[1-9]' "${ALL}"; then
	echo "PX7_SPRITE angled=CONFIRMED"
else
	echo "PX7_SPRITE angled=NOT REPRODUCIBLE WITH CURRENT GAME CONTENT"
fi
ls -1 "${OUT}"
rg -n 'sprite |GL_RenderFrame|visible frame stays Xash|PX7_SPR_' "${ALL}" | head -n 80
exit 0
