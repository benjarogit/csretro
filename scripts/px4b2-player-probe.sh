#!/usr/bin/env bash
# PX4B.3: Local player Xash eligibility + explicit player shadows on Variante B.
# PX4B.2 Remote-B regression remains. Visible frame stays Xash.
# Usage: ./scripts/build-client.sh && ./scripts/px4b2-player-probe.sh
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

fail() { echo "PX4B2_PROBE FAIL: $*" >&2; exit 1; }

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" && -f "${GAMEDLL}" && -f "${MENU}" ]] || fail "Binaries fehlen"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/de_dust.bsp" ]] || fail "de_dust.bsp fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/cs_assault.bsp" ]] || fail "cs_assault.bsp fehlt"
[[ -f "${GAMEDATA}/cstrike/maps/de_torn.bsp" ]] || fail "de_torn.bsp fehlt"

export CSRETRO_ENGINE_OUT="$(readlink -f "${ENG}")"
export CSRETRO_CLIENT_SO="$(readlink -f "${CLIENT}")"
export CSRETRO_GAMEDLL_SO="$(readlink -f "${GAMEDLL}")"
export CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
csretro_gate_isolate_begin "px4b3" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${ROOT}/build/px4b2-player-shots"
mkdir -p "${SHOTS}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
r_csretro_renderer 1
r_csretro_offscreen_dump 1
r_csretro_probe_seq 11
r_shadows 1
sv_cheats 1
allow_spectators 1
mp_forcecamera 0
mp_fadetoblack 0
mp_auto_join_team 1
humans_join_team T
mp_freezetime 0
mp_limitteams 0
mp_autoteambalance 0
bot_enable 1
bot_quota 3
bot_join_after_player 0
sv_lan 1
echo CSRETRO_PX4B2_PROBE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px4b2.log"

wait_log() {
	local pat="$1" secs="$2" i
	for i in $(seq 1 "${secs}"); do
		if rg -q "${pat}" "${LOG}" 2>/dev/null || rg -q "${pat}" "${CSRETRO_GAMESCOPE_LOG}" 2>/dev/null; then
			return 0
		fi
		sleep 1
	done
	return 1
}

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

wait_log 'offscreen world map=maps/de_aztec' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_aztec offscreen proof"
}
wait_log 'player_info hash BEFORE=' 25 || true
wait_log 'offscreen player proof' 20 || true
wait_log 'local firstperson hidden' 20 || true
wait_log 'explicit shadow r_shadows=1' 20 || true
wait_log 'probe_seq player look' 20 || true
if command -v import >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/aztec-player.png" 2>/dev/null || true
fi
wait_log 'probe_seq r_shadows 0' 25 || true
wait_log 'explicit shadow r_shadows=0' 15 || true
wait_log 'probe_seq r_shadows 1' 15 || true
wait_log 'probe_seq thirdperson' 20 || true
wait_log 'local thirdperson drawn' 20 || true
if command -v import >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/aztec-thirdperson.png" 2>/dev/null || true
fi
wait_log 'probe_seq spectator join' 20 || true
wait_log 'probe_seq spectator ineye' 15 || true
wait_log 'spectator class' 15 || true
wait_log 'probe_seq spectator chase' 15 || true
wait_log 'offscreen world map=maps/de_torn' 50 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_torn offscreen proof"
}
wait_log 'offscreen world map=maps/cs_assault' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein cs_assault offscreen proof"
}
wait_log 'offscreen world map=maps/de_dust' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_dust offscreen proof"
}
wait_log 'probe_seq vid_setmode' 25 || true
wait_log 'probe_seq quit' 25 || true
csretro_gate_wait_quit "${XASH_PID}" 20 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px4b2.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi
if rg -qi 'STUDIO_EVENTS|events=1' "${ALL}"; then
	fail "STUDIO_EVENTS darf offscreen nicht an sein"
fi
if ! rg -q 'Studio classified:.*attempted: [1-9].*drawn: [1-9]' "${ALL}"; then
	fail "PX4A-Regression: kein Non-Player Studio attempted>0 drawn>0"
fi
rg -q 'player_candidates=[1-9]|candidates=[1-9]' "${ALL}" || fail "keine Remote-Player-Kandidaten"
rg -q 'player_drawn=[1-9]|drawn=[1-9].*info_mutate=' "${ALL}" || fail "kein Remote-Player gezeichnet"
rg -q 'offscreen player proof .*differ=1' "${ALL}" || fail "kein Player-Pixel-Nachweis"
rg -q 'player_info live before=.*mutate=0' "${ALL}" || fail "live player_info mutate != 0"
rg -q 'entity_mutate=0' "${ALL}" || fail "live entity mutate != 0"
rg -q 'visible_advanced=1' "${ALL}" || fail "sichtbares Xash hat player_info nicht einmal advanced"
rg -q 'player_info hash BEFORE=' "${ALL}" || fail "kein BEFORE/AFTER_OFFSCREEN/AFTER_VISIBLE Hash"
if ! rg -q 'AFTER_OFFSCREEN=' "${ALL}"; then
	fail "AFTER_OFFSCREEN Hash fehlt"
fi
if ! rg -q 'AFTER_VISIBLE=' "${ALL}"; then
	fail "AFTER_VISIBLE Hash fehlt"
fi
if rg -q 'shadow_side_draw=1' "${ALL}"; then
	fail "unbeabsichtigter shadow side draw"
fi
rg -q 'local player class' "${ALL}" || fail "Local-Player-Klassifikation fehlt"
rg -q 'setupclientanim=0 \(m_bLocal=0\)' "${ALL}" || fail "m_bLocal/SetupClientAnimation-Doku fehlt"
rg -q 'local firstperson hidden' "${ALL}" || fail "First-Person Hidden fehlt"
rg -q 'local_drawn=0' "${ALL}" || fail "First-Person local_drawn nicht 0"
rg -q 'probe_seq thirdperson' "${ALL}" || fail "Third-Person Sequenz fehlt"
rg -q 'local thirdperson drawn' "${ALL}" || fail "Third-Person Local-Draw fehlt"
rg -q 'local thirdperson drawn .*pixel=1' "${ALL}" || fail "Third-Person Local-Pixel fehlt"
rg -q 'explicit shadow r_shadows=1 .*drawn=[1-9]' "${ALL}" || fail "r_shadows 1 ohne explicit shadow"
rg -q 'explicit shadow r_shadows=0 drawn=0' "${ALL}" || fail "r_shadows 0 ohne shadow_drawn=0"
rg -q 'explicit shadow r_shadows=1 .*differ=1' "${ALL}" || fail "Remote-Shadow-Pixel fehlt"
rg -q 'spectator class' "${ALL}" || fail "Spectator-Klassifikation fehlt"
rg -q 'probe_seq spectator ineye' "${ALL}" || fail "Spectator IN_EYE Sequenz fehlt"
rg -q 'probe_seq spectator chase' "${ALL}" || fail "Spectator CHASE Sequenz fehlt"
rg -q 'offscreen world map=maps/de_dust' "${ALL}" || fail "Mapchange dust fehlt"
rg -q 'probe_seq vid_setmode 1024 768' "${ALL}" || fail "vid_setmode fehlt"
if rg -q 'FOLLOW player-parent' "${ALL}"; then
	rg -q 'offscreen FOLLOW player-parent' "${ALL}" || fail "Player-parent FOLLOW ohne Draw-Log"
fi

echo "PX4B2_PROBE PASS"
echo "PX4B2_PROBE log=${ALL}"
rg -n 'player_info|offscreen player|local player class|local firstperson|local thirdperson|explicit shadow|spectator class|Studio classified:|FOLLOW |probe_seq|visible frame stays Xash' "${ALL}" | head -n 120 || true
exit 0
