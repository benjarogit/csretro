#!/usr/bin/env bash
# PX6A: first visible custom frame behind Mode-2 takeover gate.
# Mode 0/1 must stay return 0. Mode 2 may return 1 after Prepare/Finalize.
# Issue #12 stays OPEN until full DoD. Does not close #1/#2/#3.
# Usage: ./scripts/build-engine.sh && ./scripts/build-client.sh && \
#        ./scripts/px6a-takeover-probe.sh
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

fail() { echo "PX6A_TAKEOVER_PROBE FAIL: $*" >&2; exit 1; }

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
csretro_gate_isolate_begin "px6a" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${ROOT}/build/px6a-takeover-shots"
mkdir -p "${SHOTS}"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<'EOF'
developer 2
r_csretro_renderer 2
r_csretro_offscreen_dump 1
r_csretro_probe_seq 15
r_ripple 0
sv_cheats 1
mp_auto_join_team 1
humans_join_team T
mp_freezetime 0
mp_limitteams 0
mp_autoteambalance 0
bot_enable 1
bot_quota 1
bot_join_after_player 0
sv_lan 1
echo CSRETRO_PX6A_TAKEOVER_PROBE_CFG
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px6a.log"

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

wait_log 'PX6A takeover present=' 45 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein PX6A takeover present log"
}
if command -v import >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/aztec-mode2.png" 2>/dev/null || true
fi
wait_log 'probe_seq PX6A fire ak47' 20 || true
wait_log 'probe_seq PX6A throw he' 20 || true
wait_log 'probe_seq PX6A dev_overview 1' 25 || true
wait_log 'takeover reject reason=' 25 || true
wait_log 'probe_seq PX6A r_ripple 1' 25 || true
wait_log 'probe_seq PX6A mode 2→0' 25 || true
wait_log 'probe_seq PX6A mode 1→2' 30 || true
wait_log 'probe_seq PX6A map de_torn' 35 || true
wait_log 'probe_seq PX6A map de_dust' 50 || true
wait_log 'probe_seq PX6A vid_setmode' 30 || true
if command -v import >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
	import -window root "${SHOTS}/dust-mode2-1024.png" 2>/dev/null || true
fi
wait_log 'probe_seq PX6A quit' 25 || true
csretro_gate_wait_quit "${XASH_PID}" 20 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px6a.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true

rg -q 'GL_RenderFrame callback reached' "${ALL}" || fail "GL_RenderFrame nicht erreicht"
rg -q 'r_csretro_renderer 2 takeover candidate' "${ALL}" || fail "Mode-2 Candidate-Log fehlt"
rg -q 'PX6A takeover present=1' "${ALL}" || fail "takeover present!=1"
rg -q 'fbo=1280x720 viewport=1280x720|fbo=1024x768 viewport=1024x768' "${ALL}" || fail "FBO/viewport size mismatch"
rg -q 'framecount=[0-9]+→[0-9]+' "${ALL}" || fail "framecount log fehlt"
rg -q 'dlight=1' "${ALL}" || fail "dlight_pushes!=1"
rg -q 'efx_s=1' "${ALL}" || fail "efx solid advance fehlt"
rg -q 'efx_t=1' "${ALL}" || fail "efx trans advance fehlt"
rg -q 'tri_n=1' "${ALL}" || fail "triangle normal owned fehlt"
rg -q 'tri_t=1' "${ALL}" || fail "triangle trans owned fehlt"
rg -q 'extra=[2-9]' "${ALL}" || fail "CL_ExtraUpdate rhythm fehlt"
rg -q 'probe_seq PX6A dev_overview 1' "${ALL}" || fail "overview fallback step fehlt"
rg -q 'takeover reject reason=2' "${ALL}" || fail "overview reject (reason=2) fehlt"
rg -q 'probe_seq PX6A r_ripple 1' "${ALL}" || fail "ripple fallback step fehlt"
rg -q 'takeover reject reason=5' "${ALL}" || fail "ripple reject (reason=5) fehlt"
rg -q 'probe_seq PX6A mode 2→0' "${ALL}" || fail "mode 2→0 fehlt"
rg -q 'probe_seq PX6A mode 0→2' "${ALL}" || fail "mode 0→2 fehlt"
rg -q 'probe_seq PX6A mode 2→1' "${ALL}" || fail "mode 2→1 fehlt"
rg -q 'probe_seq PX6A mode 1→2' "${ALL}" || fail "mode 1→2 fehlt"
rg -q 'probe_seq PX6A map de_torn' "${ALL}" || fail "map torn fehlt"
rg -q 'probe_seq PX6A map cs_assault' "${ALL}" || fail "map assault fehlt"
rg -q 'probe_seq PX6A map de_dust' "${ALL}" || fail "map dust fehlt"
rg -q 'probe_seq PX6A vid_setmode 1024 768' "${ALL}" || fail "vid_setmode fehlt"
# Headless gamescope may clamp the requested mode; require a real FBO recreate
# that still matches the live viewport (not a stale 1280x720 target).
if ! python3 - "${ALL}" <<'PY'
import re, sys
text = open(sys.argv[1], errors="replace").read()
sizes = re.findall(
    r"PX6A takeover present=1 fbo=(\d+)x(\d+) viewport=(\d+)x(\d+)",
    text,
)
if len(sizes) < 2:
    sys.exit(1)
ok_match = all(int(a) == int(c) and int(b) == int(d) for a, b, c, d in sizes)
first = (int(sizes[0][0]), int(sizes[0][1]))
later = [(int(a), int(b)) for a, b, c, d in sizes[1:]]
changed = any(s != first for s in later)
sys.exit(0 if ok_match and changed else 2)
PY
then
	fail "vid_setmode FBO resize fehlt (FBO muss Viewport folgen und sich ändern)"
fi

if ! python3 - "${ALL}" <<'PY'
import re, sys
text = open(sys.argv[1], errors="replace").read()
m = re.search(
    r"PX6A takeover present=1 fbo=(\d+)x(\d+) viewport=(\d+)x(\d+) framecount=(\d+)→(\d+) dlight=(\d+)",
    text,
)
if not m:
    sys.exit(1)
fw, fh, vw, vh = map(int, m.groups()[:4])
before, after, dlight = map(int, m.groups()[4:])
if fw != vw or fh != vh:
    sys.exit(2)
if after - before != 1:
    sys.exit(3)
if dlight != 1:
    sys.exit(4)
sys.exit(0)
PY
then
	fail "framecount_delta/FBO proof unvollständig"
fi

[[ -f "${SHOTS}/aztec-mode2.png" ]] || echo "PX6A_TAKEOVER_PROBE WARN: screenshot fehlt (import?)" >&2

echo "PX6A_TAKEOVER_PROBE PASS"
echo "PX6A_TAKEOVER_PROBE log=${ALL}"
rg -n 'PX6A|takeover|probe_seq PX6A|GL_RenderFrame|reject reason' "${ALL}" | head -n 120 || true
exit 0
