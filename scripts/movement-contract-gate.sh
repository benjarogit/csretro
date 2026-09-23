#!/usr/bin/env bash
# Movement gate: client and GameDLL must share the classic CS 1.6 contract.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLIENT_H="${ROOT}/client/body/pm_shared/pm_shared.h"
SERVER_H="${ROOT}/server/game/regamedll/pm_shared/pm_shared.h"
CLIENT_C="${ROOT}/client/body/pm_shared/pm_shared.cpp"
SERVER_C="${ROOT}/server/game/regamedll/pm_shared/pm_shared.cpp"

fail() { echo "MOVEMENT_CONTRACT_GATE FAIL: $*" >&2; exit 1; }

[[ -f "${CLIENT_H}" && -f "${SERVER_H}" ]] || fail "pm_shared.h fehlt"
[[ -f "${CLIENT_C}" && -f "${SERVER_C}" ]] || fail "pm_shared.cpp fehlt"

extract() {
	local file="$1"
	local name="$2"
	local line
	line="$(rg -n "^#define[[:space:]]+${name}[[:space:]]+" "${file}" | head -n1 || true)"
	[[ -n "${line}" ]] || fail "${name} fehlt in ${file}"
	echo "${line#*#define}" | awk '{ $1=""; sub(/^ /,""); print }' | tr -d ' \t\r'
}

NAMES=(
	WJ_HEIGHT
	STOP_EPSILON
	MAX_CLIMB_SPEED
	PLAYER_DUCKING_MULTIPLIER
	PLAYER_LONGJUMP_SPEED
	TIME_TO_DUCK
	PM_VEC_DUCK_HULL_MIN
	PM_VEC_HULL_MIN
	PM_VEC_DUCK_VIEW
	PM_VEC_VIEW
	PM_PLAYER_MAX_SAFE_FALL_SPEED
	PM_PLAYER_MIN_BOUNCE_SPEED
	PM_PLAYER_FALL_PUNCH_THRESHHOLD
	BUNNYJUMP_MAX_SPEED_FACTOR
)

for name in "${NAMES[@]}"; do
	cv="$(extract "${CLIENT_H}" "${name}")"
	sv="$(extract "${SERVER_H}" "${name}")"
	[[ "${cv}" == "${sv}" ]] || fail "${name}: client='${cv}' server='${sv}'"
done

rg -q 'pmove->fuser2 = 1315\.789429' "${CLIENT_C}" || fail "Client setzt fuser2-Recovery nicht auf den Velaron-Wert"
rg -q 'pmove->fuser2 = 1315\.789429' "${SERVER_C}" || fail "GameDLL setzt fuser2-Recovery nicht auf den Velaron-Wert"
rg -q 'pmove->velocity\[0\] \*= flRatio' "${CLIENT_C}" || fail "Client wendet fuser2-Hitch in WalkMove nicht an"
rg -q 'pmove->velocity\[0\] \*= flRatio' "${SERVER_C}" || fail "GameDLL wendet fuser2-Hitch in WalkMove nicht an"

echo "MOVEMENT_CONTRACT_GATE PASS"
echo "Konstanten Client=GameDLL; fuser2-Recovery 1315.789429 ms wie Velaron beiderseits."
