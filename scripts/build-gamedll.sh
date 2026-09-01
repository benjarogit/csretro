#!/usr/bin/env bash
# Baut die eine CS-Retro-GameDLL (64-Bit) mit Clang.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=env.sh
if [[ -f "${ROOT}/scripts/env.sh" ]]; then
    source "${ROOT}/scripts/env.sh"
fi

SANITIZE=0
for arg in "$@"; do
    if [[ "$arg" == "--sanitize" ]]; then
        SANITIZE=1
    fi
done
if [[ "$SANITIZE" -eq 1 ]]; then
    OUT="${CSRETRO_GAMEDLL_OUT:-${ROOT}/build/gamedll-sanitize}"
else
    OUT="${CSRETRO_GAMEDLL_OUT:-${ROOT}/build/gamedll-cmake}"
fi

mkdir -p "${OUT}"
export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"

EXTRA=()
if [[ "$SANITIZE" -eq 1 ]]; then
    EXTRA+=(-DCSRETRO_GAMEDLL_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug)
else
    EXTRA+=(-DCSRETRO_GAMEDLL_SANITIZE=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo)
fi

cmake -S "${ROOT}" -B "${OUT}" -G Ninja \
    -DCSRETRO_BUILD_ENGINE=OFF \
    -DCSRETRO_BUILD_CLIENT=OFF \
    -DCSRETRO_BUILD_XASH_CLIENT=OFF \
    -DCSRETRO_BUILD_GAMEDLL=ON \
    "${EXTRA[@]}"

cmake --build "${OUT}" --target csretro_gamedll -j"$(nproc)"
echo "Fertig. GameDLL unter ${OUT}/"
