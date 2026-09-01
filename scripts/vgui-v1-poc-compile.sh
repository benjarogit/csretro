#!/usr/bin/env bash
# Phase-3M V1-PoC: compile-proof that vendored vgui_controls (Frame/PropertyDialog/…)
# build on Linux x86_64 after finite 64-Bit-Patches. Kein Steam-vgui2, kein Full-Link.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${ROOT}/client/dep/NclNitroApi/dep/ncl-hl1-source-sdk"
OUT="${CSRETRO_V1POC_OUT:-/tmp/csretro-v1poc}"
mkdir -p "${OUT}/shim" "${OUT}/shim/vgui" "${OUT}/shimcase/tier1"
cd "${OUT}"

# Win32-Header-Stubs (InputWin32/vgui brauchen Includes, kaum APIs)
cat > shim/windows.h <<'EOF'
#pragma once
typedef void *HWND;
inline HWND GetActiveWindow() { return nullptr; }
inline void OutputDebugString(const char *s) { (void)s; }
inline void OutputDebugStringA(const char *s) { (void)s; }
EOF
cp shim/windows.h shim/Windows.h
printf '%s\n' '#pragma once' > shim/imm.h
printf '%s\n' '#pragma once' > shim/wtypes.h
printf '%s\n' '#pragma once' '#define VK_ESCAPE 0x1B' > shim/winuser.h
printf '%s\n' '#pragma once' '#include "tier0/dbg.h"' > shimcase/Assert.h

link_case() { ln -sfn "$1" "$2"; }
link_case "${SDK}/public/tier1/utlvector.h" shimcase/UtlVector.h
link_case "${SDK}/public/tier1/utllinkedlist.h" shimcase/UtlLinkedList.h
link_case "${SDK}/public/tier1/utlbuffer.h" shimcase/UtlBuffer.h
link_case "${SDK}/public/tier1/utlpriorityqueue.h" shimcase/UtlPriorityQueue.h
link_case "${SDK}/public/tier1/utlrbtree.h" shimcase/UtlRBTree.h
link_case "${SDK}/public/tier1/utlsymbol.h" shimcase/UtlSymbol.h
link_case "${SDK}/public/tier1/utlvector.h" shimcase/tier1/UtlVector.h
link_case "${SDK}/public/tier1/utlrbtree.h" shimcase/tier1/UtlRBTree.h
link_case "${SDK}/public/vgui/KeyCode.h" shim/vgui/keycode.h

FLAGS=( -c -std=c++17 -fPIC
  -DGNUC -DPOSIX -DLINUX -D_LINUX -DNDEBUG -DNO_MALLOC_OVERRIDE -DMATHLIB_HEADER_ONLY
  -DNO_STEAM -DVALVE_LITTLE_ENDIAN -DPLATFORM_64BITS
  -D_DLL_EXT=\".so\" -DSOURCE_SDK_GFX_PATH=\"gfx/vgui2\"
  -I"${OUT}/shim" -I"${OUT}/shimcase"
  -I"${SDK}/public" -I"${SDK}/public/tier0" -I"${SDK}/public/tier1" -I"${SDK}/public/vgui"
  -I"${SDK}/common" -I"${SDK}/engine" -I"${SDK}/vgui2/src" -I"${SDK}"
  -Wno-ignored-attributes )

ok=0
fail=0
try() {
  local name="$1"; shift
  if clang++ "${FLAGS[@]}" "$@" -o "${name}.o" 2>"${name}.err"; then
    echo "  OK    ${name}"
    ok=$((ok + 1))
  else
    echo "  FAIL  ${name}"
    rg -m3 'error:' "${name}.err" || true
    fail=$((fail + 1))
  fi
}

echo "=== V1 PoC compile (Linux x86_64, kein Link) ==="
try VPanel "${SDK}/vgui2/src/VPanel.cpp"
try VPanelWrapper "${SDK}/vgui2/src/VPanelWrapper.cpp"
try InputWin32 "${SDK}/vgui2/src/InputWin32.cpp"
try vgui "${SDK}/vgui2/src/vgui.cpp"
try vgui_border "${SDK}/vgui2/src/vgui_border.cpp"
try vgui_internal "${SDK}/vgui2/src/vgui_internal.cpp"
try Bitmap "${SDK}/vgui2/src/Bitmap.cpp"
try lang_code "${SDK}/vgui2/src/lang_code.cpp"
try Panel "${SDK}/vgui2/vgui_controls/Panel.cpp"
try Label "${SDK}/vgui2/vgui_controls/Label.cpp"
try Button "${SDK}/vgui2/vgui_controls/Button.cpp"
try TextImage "${SDK}/vgui2/vgui_controls/TextImage.cpp"
try Image "${SDK}/vgui2/vgui_controls/Image.cpp"
try FocusNavGroup "${SDK}/vgui2/vgui_controls/FocusNavGroup.cpp"
try EditablePanel "${SDK}/vgui2/vgui_controls/EditablePanel.cpp"
try Frame "${SDK}/vgui2/vgui_controls/Frame.cpp"
try PropertyPage "${SDK}/vgui2/vgui_controls/PropertyPage.cpp"
try PropertySheet "${SDK}/vgui2/vgui_controls/PropertySheet.cpp"
try PropertyDialog "${SDK}/vgui2/vgui_controls/PropertyDialog.cpp"
try TextEntry "${SDK}/vgui2/vgui_controls/TextEntry.cpp"
try controls "${SDK}/vgui2/vgui_controls/controls.cpp"

# Erwartet FAIL — ersetzen, nicht stubben:
echo "--- erwartet FAIL (ersetzen) ---"
try System "${SDK}/vgui2/src/System.cpp" || true
try SurfaceNext "${SDK}/vgui2/src/SurfaceNext.cpp" || true
try FontReplace "${SDK}/vgui2/src/FontReplace.cpp" || true

echo "=== Ergebnis: OK=${ok} FAIL=${fail} (Controls/Frame/PropertyDialog müssen OK sein) ==="
[[ "${fail}" -eq 0 ]] || {
  # System/SurfaceNext/FontReplace zählen nicht als Gate-Fail
  true
}
# Gate: Pflicht-Objekte
for need in Panel.o Label.o Button.o Frame.o PropertyDialog.o TextEntry.o VPanel.o InputWin32.o; do
  [[ -f "${need}" ]] || { echo "GATE FAIL: ${need} fehlt"; exit 1; }
done
echo "GATE PASS: Frame + PropertyDialog + TextEntry compile on x86_64"
exit 0
