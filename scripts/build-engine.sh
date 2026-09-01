#!/usr/bin/env bash
# Baut nur die Ziel-Engine (Xash3D-FWGS) mit aktuellem Clang.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=env.sh
source "${ROOT}/scripts/env.sh"

OUT="${CSRETRO_ENGINE_OUT:-${ROOT}/build/engine}"
mkdir -p "${OUT}"

cd "${ROOT}/engine"

echo "CS Retro: Waf configure (Clang, 64-bit) → ${OUT}"
python3 ./waf configure -8 --out "${OUT}" \
  --check-c-compiler clang --check-cxx-compiler clang++ \
  "$@"

echo "CS Retro: Waf build"
python3 ./waf build --out "${OUT}"
echo "Fertig. Binaries unter ${OUT}"
