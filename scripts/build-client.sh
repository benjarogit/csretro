#!/usr/bin/env bash
# Baut die eine CS-Retro-Client-Lib (Export + A1-Body) mit Clang.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=env.sh
if [[ -f "${ROOT}/scripts/env.sh" ]]; then
    source "${ROOT}/scripts/env.sh"
fi

OUT="${CSRETRO_CLIENT_OUT:-${ROOT}/build/client-cmake}"
mkdir -p "${OUT}"

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"

cmake -S "${ROOT}" -B "${OUT}" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCSRETRO_BUILD_ENGINE=OFF \
    -DCSRETRO_BUILD_CLIENT=OFF \
    -DCSRETRO_BUILD_XASH_CLIENT=ON \
    "$@"

cmake --build "${OUT}" --target csretro_client -j"$(nproc)"
echo "Fertig. Client unter ${OUT}/client/"
