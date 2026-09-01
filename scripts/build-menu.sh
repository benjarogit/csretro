#!/usr/bin/env bash
# Baut die CS-Retro-Menü-Lib (GetMenuAPI + GameMenuExports001).
#
# Usage:
#   ./scripts/build-menu.sh
#   ./scripts/build-menu.sh --sanitize   # ASan+UBSan → build/menu-sanitize/
# Weitere CMake-Args werden durchgereicht.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "${ROOT}/scripts/env.sh" ]]; then
    # shellcheck source=env.sh
    source "${ROOT}/scripts/env.sh"
fi

SANITIZE=0
PASSTHRU=()
for arg in "$@"; do
    if [[ "$arg" == "--sanitize" ]]; then
        SANITIZE=1
    else
        PASSTHRU+=("$arg")
    fi
done

if [[ "${SANITIZE}" -eq 1 ]]; then
    OUT="${CSRETRO_MENU_OUT:-${ROOT}/build/menu-sanitize}"
else
    OUT="${CSRETRO_MENU_OUT:-${ROOT}/build/client-cmake}"
fi
mkdir -p "${OUT}"

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"

EXTRA=()
if [[ "${SANITIZE}" -eq 1 ]]; then
    EXTRA+=(
        -DCMAKE_BUILD_TYPE=Debug
        -DCSRETRO_MENU_SANITIZE=ON
        -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
        -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
        -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined'
    )
else
    EXTRA+=(-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCSRETRO_MENU_SANITIZE=OFF)
fi

cmake -S "${ROOT}" -B "${OUT}" -G Ninja \
    "${EXTRA[@]}" \
    -DCSRETRO_BUILD_ENGINE=OFF \
    -DCSRETRO_BUILD_CLIENT=OFF \
    -DCSRETRO_BUILD_XASH_CLIENT=ON \
    -DCSRETRO_BUILD_MENU=ON \
    "${PASSTHRU[@]}"

cmake --build "${OUT}" --target csretro_menu -j"$(nproc)"
echo "Fertig. Menü unter ${OUT}/menu/"
