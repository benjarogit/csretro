#!/usr/bin/env bash
# PX4C.2: Viewmodel event ownership. Client and Xash share one Once-gate.
# PX4C.1 body stays VERIFIED. GL_RenderFrame stays 0. No Vis / return 1.
# Usage: ./scripts/build-engine.sh && ./scripts/build-client.sh && \
#        ./scripts/px4c2-viewmodel-events-probe.sh
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

fail() { echo "PX4C2_PROBE FAIL: $*" >&2; exit 1; }

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
csretro_gate_isolate_begin "px4c2" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${ROOT}/build/px4c2-viewmodel-shots"
mkdir -p "${SHOTS}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
r_csretro_renderer 0
r_csretro_offscreen_dump 1
r_csretro_probe_seq 13
r_drawviewmodel 1
cl_righthand 1
sv_cheats 1
mp_forcecamera 0
mp_fadetoblack 0
mp_auto_join_team 1
humans_join_team T
mp_freezetime 0
mp_limitteams 0
mp_autoteambalance 0
bot_enable 1
bot_quota 2
bot_join_after_player 0
sv_lan 1
echo CSRETRO_PX4C2_PROBE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px4c2.log"

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

wait_log 'probe_seq events renderer 0' 40 || true
wait_log 'viewmodel events owner=xash' 20 || true
wait_log 'probe_seq events renderer 1' 20 || true
wait_log 'viewmodel events owner=client' 20 || true
wait_log 'offscreen world map=maps/de_aztec' 40 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein de_aztec offscreen proof"
}
wait_log 'offscreen viewmodel proof' 25 || true
wait_log 'viewmodel events claim client_claims=1' 15 || true
wait_log 'viewmodel events double-call' 10 || true
wait_log 'probe_seq viewmodel knife' 20 || true
if command -v import >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/aztec-viewmodel.png" 2>/dev/null || true
fi
wait_log 'probe_seq viewmodel fire' 25 || true
wait_log 'viewmodel events muzzle' 15 || true
wait_log 'probe_seq viewmodel molotov wick' 40 || true
if command -v import >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/aztec-molotov.png" 2>/dev/null || true
fi
wait_log 'viewmodel events wick' 15 || true
wait_log 'probe_seq r_drawviewmodel 0' 20 || true
wait_log 'viewmodel events claim rejected' 15 || true
wait_log 'probe_seq r_drawviewmodel 1' 15 || true
wait_log 'probe_seq viewmodel thirdperson' 15 || true
wait_log 'probe_seq viewmodel dead' 20 || true
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

ALL="${RUN}/merged-px4c2.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'visible frame stays Xash|Xash fallback selected' "${ALL}" || fail "Xash-Fallback nicht geloggt"
if rg -qi 'GL_RenderFrame.*return 1|custom path took over' "${ALL}"; then
	fail "GL_RenderFrame darf nicht 1 zurückgeben"
fi
if rg -q 'event_impl_runs=2' "${ALL}"; then
	fail "Event-Impl darf nicht 2× laufen"
fi
rg -q 'viewmodel events owner=xash client_claims=0 .*event_impl_runs=1' "${ALL}" || fail "Renderer 0: Xash ist nicht Event-Owner"
rg -q 'viewmodel events owner=client client_claims=1 .*event_impl_runs=1 .*default_duplicate_skips=1' "${ALL}" || fail "Renderer 1: Client-Claim / Default-Skip fehlt"
rg -q 'viewmodel events claim client_claims=1 first_rc=1 event_impl_runs=1' "${ALL}" || fail "Client-Claim-Log fehlt"
rg -q 'viewmodel events double-call first=1 second=-1 attach_b_eq_c=1|viewmodel events double-call first=0 second=-1 attach_b_eq_c=1' "${ALL}" || fail "Double-Call-Stress fehlt"
rg -q 'b_eq_c=1|attach_b_eq_c=1' "${ALL}" || fail "Attachments nicht exactly-once"
rg -q 'currententity_restore=1' "${ALL}" || fail "CurrentEntity restore fehlt"
rg -q 'gl_restore=1' "${ALL}" || fail "Event-GL-Restore fehlt"
rg -q 'viewmodel events claim rejected .*event_impl_runs=0' "${ALL}" || fail "Eligibility-Reject (r_drawviewmodel 0) fehlt"
rg -q 'offscreen viewmodel proof .*differ=1' "${ALL}" || fail "PX4C.1 Body-Pixel fehlt"
rg -q 'viewmodel live hash .*live_mutate=0' "${ALL}" || fail "live VM body mutate != 0"
rg -q 'righthand_mutate=0' "${ALL}" || fail "cl_righthand mutate != 0"
rg -q 'viewmodel depth .*restore=1' "${ALL}" || fail "DepthRange restore fehlt"
if rg -q 'offscreen viewmodel proof .*events=1|viewmodel live hash .*events=1|viewmodel molotov .*events=1' "${ALL}"; then
	fail "Offscreen BODY darf keine STUDIO_EVENTS haben"
fi
rg -q 'viewmodel r_drawviewmodel 0 .*drawn_frame=0' "${ALL}" || fail "r_drawviewmodel 0 ohne drawn_frame=0"
rg -q 'viewmodel thirdperson .*drawn_frame=0' "${ALL}" || fail "Thirdperson ohne drawn_frame=0"
rg -q 'viewmodel dead-player .*drawn_frame=0' "${ALL}" || fail "Dead-Player ohne drawn_frame=0"
rg -q 'offscreen world map=maps/de_dust' "${ALL}" || fail "Mapchange dust fehlt"
rg -q 'probe_seq vid_setmode 1024 768' "${ALL}" || fail "vid_setmode fehlt"
if rg -q 'viewmodel molotov candidate=1|event_wick_capture=[1-9]' "${ALL}"; then
	rg -q 'offscreen_body_wick_capture=0|wick_captures=0' "${ALL}" || fail "Offscreen-body Wick capture != 0"
	rg -q 'event_wick_capture=[1-9]' "${ALL}" || fail "Event-phase Wick capture fehlt"
	rg -q 'held_advances=[1-9]' "${ALL}" || fail "EV_UpdateMolotovHeld läuft nicht"
	if rg -q 'held-update source=captured|source=captured age=' "${ALL}"; then
		echo "PX4C2_PROBE molotov held source=captured"
	else
		echo "PX4C2_PROBE molotov held source=fallback (HUD weapon/key; existing held semantics, not #2)"
	fi
fi
if rg -q 'alias_seen=[1-9]|alias=1' "${ALL}"; then
	fail "Unerwartetes Alias-Viewmodel ohne Follow-up"
fi

echo "PX4C2_PROBE PASS"
echo "PX4C2_PROBE log=${ALL}"
rg -n 'viewmodel events|viewmodel molotov|offscreen viewmodel|probe_seq events|probe_seq viewmodel|visible frame stays Xash|Xash fallback' "${ALL}" | head -n 160 || true
exit 0
