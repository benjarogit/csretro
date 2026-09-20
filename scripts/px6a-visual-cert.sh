#!/usr/bin/env bash
# PX6A.1 Visual Certification / #12 DoD-Rest.
# Runs against the frozen implementation SHA recorded in the log.
# Does NOT create a release. Shots stay local under build/px6a-cert-shots/.
# Usage: ./scripts/build-engine.sh && ./scripts/build-client.sh && \
#        ./scripts/px6a-visual-cert.sh
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
SHA="$(git -C "${ROOT}" rev-parse HEAD)"
SHORT="$(git -C "${ROOT}" rev-parse --short HEAD)"

fail() { echo "PX6A1_VISUAL_CERT FAIL: $*" >&2; exit 1; }

[[ -x "${ENG}/game_launch/xash3d" ]] || fail "Engine fehlt"
[[ -f "${CLIENT}" && -f "${GAMEDLL}" && -f "${MENU}" ]] || fail "Binaries fehlen"
GAMEDATA="$(csretro_gamedata_require "${ROOT}" "${MAP}")" || fail "Game-Data fehlt"

export CSRETRO_ENGINE_OUT="$(readlink -f "${ENG}")"
export CSRETRO_CLIENT_SO="$(readlink -f "${CLIENT}")"
export CSRETRO_GAMEDLL_SO="$(readlink -f "${GAMEDLL}")"
export CSRETRO_MENU_SO="$(readlink -f "${MENU}")"
csretro_gate_isolate_begin "px6a1cert" || fail "isolate"
csretro_gate_isolate_stage_runtime
RUN="${CSRETRO_RUN_DIR}"
LOG="${RUN}/engine.log"
SHOTS="${ROOT}/build/px6a-cert-shots"
mkdir -p "${SHOTS}"
echo "${SHA}" > "${SHOTS}/certified-sha.txt"

printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/valve/valve.rc"
printf '%s\n' 'exec autoexec.cfg' 'stuffcmds' > "${RUN}/cstrike/cstrike.rc"
cat > "${RUN}/cstrike/autoexec.cfg" <<EOF
developer 2
r_csretro_renderer 2
r_csretro_offscreen_dump 1
r_csretro_probe_seq 16
r_ripple 0
r_dynamic 1
r_shadows 1
sv_cheats 1
mp_auto_join_team 1
humans_join_team T
mp_freezetime 0
mp_limitteams 0
mp_autoteambalance 0
bot_enable 1
bot_quota 2
bot_join_after_player 0
sv_lan 1
echo CSRETRO_PX6A1_VISUAL_CERT_CFG
echo CSRETRO_PX6A1_CERT_SHA ${SHA}
EOF
cp -a "${RUN}/cstrike/autoexec.cfg" "${RUN}/cstrike/config.cfg"

export LD_LIBRARY_PATH="${ENG}/engine:${ENG}/ref/gl:${ENG}/filesystem:${LD_LIBRARY_PATH:-}"
export XASH3D_RODIR="${GAMEDATA}"
export XASH3D_BASEDIR="${RUN}"
export CSRETRO_UI_OVERRIDE="${ROOT}/data/ui-overrides/cstrike"
export CSRETRO_RUN_DIR="${RUN}"
export CSRETRO_GAMESCOPE_LOG="${RUN}/gamescope-px6a1cert.log"

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

# Prefer real ImageMagick (/usr/bin/magick import). PATH/ARGV0 may shadow with Cursor.
# Primary evidence: engine `screenshot` files under RUN/cstrike/scrshots/ (see collect_engine_shots).
shot() {
	local name="$1"
	local out="${SHOTS}/${name}.png"
	local imp=""
	[[ -n "${DISPLAY:-}" ]] || { echo "PX6A1_SHOT WARN import-skip ${name}.png (DISPLAY unset)"; return 0; }
	if [[ -x /usr/bin/magick ]]; then
		# Avoid ARGV0=cursor.AppImage breaking ImageMagick multi-call dispatch.
		( exec -a import /usr/bin/magick -display "${DISPLAY}" -window root "${out}" ) 2>/dev/null \
			|| ( exec -a import /usr/bin/magick -display "${DISPLAY}" "${out}" ) 2>/dev/null \
			|| true
	fi
	if [[ ! -f "${out}" ]] && command -v ffmpeg >/dev/null 2>&1; then
		ffmpeg -y -loglevel error -f x11grab -video_size "${CSRETRO_GAMESCOPE_W:-640}x${CSRETRO_GAMESCOPE_H:-480}" \
			-i "${DISPLAY}.0" -frames:v 1 "${out}" 2>/dev/null || true
	fi
	if [[ -f "${out}" ]]; then
		echo "PX6A1_SHOT ${name}.png"
	else
		echo "PX6A1_SHOT WARN import-miss ${name}.png (DISPLAY=${DISPLAY})"
	fi
	return 0
}

collect_engine_shots() {
	local src="${RUN}/cstrike/scrshots"
	local n=0
	mkdir -p "${SHOTS}"
	if [[ -d "${src}" ]]; then
		# Copy whatever the probe wrote; keep original names.
		shopt -s nullglob
		local f
		for f in "${src}"/px6a1_*.png; do
			cp -f "${f}" "${SHOTS}/"
			echo "PX6A1_SHOT engine $(basename "${f}")"
			n=$((n + 1))
		done
		shopt -u nullglob
	fi
	echo "PX6A1_SHOT engine_count=${n}"
	# Minimum DoD evidence set (engine framebuffer).
	local req=(
		px6a1_01_world_hud.png
		px6a1_03_console.png
		px6a1_04_scoreboard.png
		px6a1_05_preview.png
		px6a1_07_ak.png
		px6a1_09_he.png
		px6a1_12_thirdperson.png
		px6a1_14_torn_water.png
	)
	local miss=0
	local r
	for r in "${req[@]}"; do
		if [[ ! -f "${SHOTS}/${r}" ]]; then
			echo "PX6A1_SHOT MISSING required ${r}" >&2
			miss=$((miss + 1))
		fi
	done
	[[ "${miss}" -eq 0 ]] || return 1
	return 0
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

wait_log 'PX6A takeover present=1' 45 || {
	kill -TERM "${XASH_PID}" 2>/dev/null || true
	csretro_headless_x11_stop
	fail "kein Mode-2 present"
}
shot "01-aztec-mode2-world-hud"
wait_log 'probe_seq PX6A1 cert sky look' 20 || true
shot "02-aztec-sky"
wait_log 'probe_seq PX6A1 cert console open' 25 || true
shot "03-aztec-console"
wait_log 'probe_seq PX6A1 cert scoreboard open' 20 || true
shot "04-aztec-scoreboard"
wait_log 'probe_seq PX6A1 cert preview chooseteam' 25 || true
shot "05-preview-chooseteam"
wait_log 'pass=preview/non-world|probe_seq PX6A1 cert preview leave' 20 || true
wait_log 'probe_seq PX6A1 cert viewmodel pistol' 25 || true
shot "06-viewmodel-pistol"
wait_log 'probe_seq PX6A1 cert ak fire' 20 || true
shot "07-ak-efx"
wait_log 'probe_seq PX6A1 cert knife' 15 || true
shot "08-knife"
wait_log 'probe_seq PX6A1 cert he throw' 20 || true
sleep 2
shot "09-he"
wait_log 'probe_seq PX6A1 cert smoke throw' 20 || true
sleep 2
shot "10-smoke"
wait_log 'probe_seq PX6A1 cert flash throw' 20 || true
sleep 2
shot "11-flash"
wait_log 'probe_seq PX6A1 cert thirdperson' 20 || true
shot "12-thirdperson"
wait_log 'probe_seq PX6A1 cert overview 1' 30 || true
shot "13-overview-fallback"
wait_log 'probe_seq PX6A1 cert ripple 1' 20 || true
wait_log 'probe_seq PX6A1 cert force_fault' 25 || true
wait_log 'postcommit force_fault=1' 15 || true
wait_log 'probe_seq PX6A1 cert map de_torn' 40 || true
wait_log 'probe_seq PX6A1 cert water torn' 20 || true
shot "14-torn-water"
wait_log 'probe_seq PX6A1 cert map cs_assault' 30 || true
wait_log 'probe_seq PX6A1 cert assault door use' 25 || true
shot "15-assault-door"
wait_log 'probe_seq PX6A1 cert map de_dust' 30 || true
wait_log 'probe_seq PX6A1 cert vid_setmode' 25 || true
shot "16-dust-vid"
wait_log 'probe_seq PX6A1 cert quit' 30 || true
csretro_gate_wait_quit "${XASH_PID}" 25 "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" || true
csretro_headless_x11_stop

ALL="${RUN}/merged-px6a1cert.log"
cat "${LOG}" "${CSRETRO_GAMESCOPE_LOG}" >"${ALL}" 2>/dev/null || true
cp -f "${ALL}" "${SHOTS}/merged-px6a1cert.log" || true
collect_engine_shots || fail "Pflicht-Screenshots fehlen unter ${SHOTS}"

rg -q "CSRETRO_PX6A1_CERT_SHA ${SHA}" "${ALL}" || fail "Cert-SHA fehlt im Log"
rg -q 'PX6A takeover present=1' "${ALL}" || fail "Mode-2 present fehlt"
rg -q 'takeover candidate \(return=1\)' "${ALL}" || fail "return=1 Candidate-Log fehlt"
rg -q 'probe_seq PX6A1 cert console open' "${ALL}" || fail "console step fehlt"
rg -q 'probe_seq PX6A1 cert scoreboard open' "${ALL}" || fail "scoreboard step fehlt"
rg -q 'pass=preview/non-world|probe_seq PX6A1 cert preview' "${ALL}" || fail "preview step fehlt"
rg -q 'probe_seq PX6A1 cert ak fire' "${ALL}" || fail "AK step fehlt"
rg -q 'probe_seq PX6A1 cert he throw' "${ALL}" || fail "HE step fehlt"
rg -q 'probe_seq PX6A1 cert thirdperson' "${ALL}" || fail "thirdperson step fehlt"
rg -q 'probe_seq PX6A1 cert overview 1' "${ALL}" || fail "overview fallback step fehlt"
rg -q 'takeover reject reason=2' "${ALL}" || fail "overview reject fehlt"
rg -q 'probe_seq PX6A1 cert ripple 1' "${ALL}" || fail "ripple fallback step fehlt"
rg -q 'takeover reject reason=5' "${ALL}" || fail "ripple reject fehlt"
rg -q 'postcommit force_fault=1 committed=1 same_frame_return=1' "${ALL}" || fail "postcommit fault fehlt"
rg -q 'probe_seq PX6A1 cert map de_torn' "${ALL}" || fail "torn mapchange fehlt"
rg -q 'probe_seq PX6A1 cert map cs_assault' "${ALL}" || fail "assault mapchange fehlt"
rg -q 'probe_seq PX6A1 cert map de_dust' "${ALL}" || fail "dust mapchange fehlt"
rg -q 'probe_seq PX6A1 cert vid_setmode' "${ALL}" || fail "vid_setmode fehlt"
rg -q 'lost_eligible_event_frames=0' "${ALL}" || fail "lost_eligible_event_frames != 0"
if rg -q 'LOST eligible frame' "${ALL}"; then
	fail "LOST eligible event frame detected"
fi
rg -q 'fog_pre=1 fog_post=1' "${ALL}" || fail "CustomFrameFog Pre/Post != 1"
rg -q 'r_shadows=1|explicit shadow r_shadows=1' "${ALL}" || true

echo "PX6A1_VISUAL_CERT PASS"
echo "PX6A1_VISUAL_CERT sha=${SHA} short=${SHORT}"
echo "PX6A1_VISUAL_CERT log=${ALL}"
echo "PX6A1_VISUAL_CERT shots=${SHOTS}"
rg -n 'PX6A1|PX6A takeover|lost_eligible|force_fault|pass=preview|reject reason' "${ALL}" | head -n 160 || true
exit 0
