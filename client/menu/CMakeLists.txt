# CS-Retro-Menü: Xash GetMenuAPI + V1 VGUI2 (static) + GameMenuExports001.
# Keine Steam-vgui2 / GameUI / SteamAPI Runtime-Abhängigkeit.
project(csretro_menu LANGUAGES CXX)

option(CSRETRO_MENU_SANITIZE "ASan+UBSan (Entwicklungsbuild der Menü-Lib)" OFF)

set(BODY "${CSRETRO_ROOT}/client/body")
set(SDK "${CSRETRO_ROOT}/client/dep/NclNitroApi/dep/ncl-hl1-source-sdk")
set(VGUI "${CMAKE_CURRENT_SOURCE_DIR}/vgui")
set(SHIM "${VGUI}/shim")
set(SHIMCASE "${CMAKE_CURRENT_BINARY_DIR}/vgui_shimcase")

file(MAKE_DIRECTORY "${SHIMCASE}/tier1" "${SHIMCASE}/vgui")
file(CREATE_LINK "${SDK}/public/tier1/utlvector.h" "${SHIMCASE}/UtlVector.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utllinkedlist.h" "${SHIMCASE}/UtlLinkedList.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlbuffer.h" "${SHIMCASE}/UtlBuffer.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlpriorityqueue.h" "${SHIMCASE}/UtlPriorityQueue.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlrbtree.h" "${SHIMCASE}/UtlRBTree.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlsymbol.h" "${SHIMCASE}/UtlSymbol.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlvector.h" "${SHIMCASE}/tier1/UtlVector.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlrbtree.h" "${SHIMCASE}/tier1/UtlRBTree.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlsymbol.h" "${SHIMCASE}/tier1/UtlSymbol.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier1/utlstring.h" "${SHIMCASE}/tier1/UtlString.h" SYMBOLIC)
file(CREATE_LINK "${SDK}/public/tier0/dbg.h" "${SHIMCASE}/Assert.h" SYMBOLIC)
file(CREATE_LINK "${SHIM}/windows.h" "${SHIMCASE}/Windows.h" SYMBOLIC)
# Case-insensitive KeyCode include used by some VGUI headers
file(CREATE_LINK "${SDK}/public/vgui/KeyCode.h" "${SHIMCASE}/vgui/keycode.h" SYMBOLIC)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED freetype2)

set(_vgui_core
    "${SDK}/vgui2/src/VPanel.cpp"
    "${SDK}/vgui2/src/VPanelWrapper.cpp"
    "${SDK}/vgui2/src/vgui.cpp"
    "${SDK}/vgui2/src/vgui_border.cpp"
    "${SDK}/vgui2/src/Bitmap.cpp"
    "${SDK}/vgui2/src/Scheme.cpp"
    "${SDK}/vgui2/src/LocalizedStringTable.cpp"
    "${SDK}/vgui2/src/lang_code.cpp"
)

set(_vgui_controls
    "${SDK}/vgui2/vgui_controls/controls.cpp"
    "${SDK}/vgui2/vgui_controls/Panel.cpp"
    "${SDK}/vgui2/vgui_controls/Label.cpp"
    "${SDK}/vgui2/vgui_controls/Button.cpp"
    "${SDK}/vgui2/vgui_controls/TextImage.cpp"
    "${SDK}/vgui2/vgui_controls/Image.cpp"
    "${SDK}/vgui2/vgui_controls/ImagePanel.cpp"
    "${SDK}/vgui2/vgui_controls/ImageList.cpp"
    "${SDK}/vgui2/vgui_controls/FocusNavGroup.cpp"
    "${SDK}/vgui2/vgui_controls/EditablePanel.cpp"
    "${SDK}/vgui2/vgui_controls/Frame.cpp"
    "${SDK}/vgui2/vgui_controls/TextEntry.cpp"
    "${SDK}/vgui2/vgui_controls/Menu.cpp"
    "${SDK}/vgui2/vgui_controls/MenuItem.cpp"
    "${SDK}/vgui2/vgui_controls/MenuButton.cpp"
    "${SDK}/vgui2/vgui_controls/AnimationController.cpp"
    "${SDK}/vgui2/vgui_controls/BuildGroup.cpp"
    "${SDK}/vgui2/vgui_controls/BuildModeDialog.cpp"
    "${SDK}/vgui2/vgui_controls/BuildFactoryHelper.cpp"
    "${SDK}/vgui2/vgui_controls/CheckButton.cpp"
    "${SDK}/vgui2/vgui_controls/ToggleButton.cpp"
    "${SDK}/vgui2/vgui_controls/RadioButton.cpp"
    "${SDK}/vgui2/vgui_controls/ComboBox.cpp"
    "${SDK}/vgui2/vgui_controls/ScrollBar.cpp"
    "${SDK}/vgui2/vgui_controls/ScrollBarSlider.cpp"
    "${SDK}/vgui2/vgui_controls/Divider.cpp"
    "${SDK}/vgui2/vgui_controls/ProgressBar.cpp"
    "${SDK}/vgui2/vgui_controls/MessageBox.cpp"
    "${SDK}/vgui2/vgui_controls/QueryBox.cpp"
    "${SDK}/vgui2/vgui_controls/Tooltip.cpp"
    "${SDK}/vgui2/vgui_controls/ExpandButton.cpp"
    "${SDK}/vgui2/vgui_controls/URLLabel.cpp"
    "${SDK}/vgui2/vgui_controls/RichText.cpp"
    "${SDK}/vgui2/vgui_controls/BitmapImagePanel.cpp"
    "${SDK}/vgui2/vgui_controls/AnimatingImagePanel.cpp"
    "${SDK}/vgui2/vgui_controls/KeyRepeat.cpp"
    "${SDK}/vgui2/vgui_controls/PropertyPage.cpp"
    "${SDK}/vgui2/vgui_controls/PropertySheet.cpp"
    "${SDK}/vgui2/vgui_controls/PropertyDialog.cpp"
    "${SDK}/vgui2/vgui_controls/Slider.cpp"
    "${SDK}/vgui2/vgui_controls/PanelListPanel.cpp"
)

set(_tier1
    "${SDK}/tier1/KeyValues.cpp"
    "${SDK}/tier1/strtools.cpp"
    "${SDK}/tier1/strtools_unicode.cpp"
    "${SDK}/tier1/utlsymbol.cpp"
    "${SDK}/tier1/utlbuffer.cpp"
    "${SDK}/tier1/mempool.cpp"
    "${SDK}/tier1/characterset.cpp"
    "${SDK}/tier1/byteswap.cpp"
    "${SDK}/tier1/generichash.cpp"
    "${SDK}/tier1/utlstring.cpp"
    "${SDK}/tier1/qsort_s.cpp"
    "${SDK}/public/filesystem_helpers.cpp"
)

list(APPEND _vgui_controls
    "${SDK}/vgui2/vgui_controls/ToolWindow.cpp"
)

add_library(csretro_menu SHARED
    src/export.cpp
    src/gameui.cpp
    src/res.cpp
    src/localize.cpp
    src/profile.cpp
    src/exports_iface.cpp
    "${BODY}/common/interface.cpp"
    "${VGUI}/interface_init.cpp"
    # V1 backends (CS Retro)
    "${VGUI}/tier0_compat.cpp"
    "${VGUI}/keyvalues_system.cpp"
    "${VGUI}/filesystem_xash.cpp"
    "${VGUI}/system_xash.cpp"
    $<IF:$<BOOL:${WIN32}>,${VGUI}/system_shell_win.cpp,${VGUI}/system_shell_posix.cpp>
    "${VGUI}/surface_xash.cpp"
    "${VGUI}/vgui_symbols.cpp"
    "${VGUI}/font_resolver.cpp"
    "${VGUI}/input_core.cpp"
    "${VGUI}/key_translation_xash.cpp"
    "${VGUI}/commandline_xash.cpp"
    "${VGUI}/vgui_boot.cpp"
    "${VGUI}/poc_dialog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/OptionsDialog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/OptionsSubMouse.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/OptionsMouseGate.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/OptionsSubAudio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/OptionsAudioGate.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/Controls/MenuEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/Controls/CvarToggleCheckButton.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/Controls/CvarNegateCheckButton.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/Controls/CvarSlider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/Controls/CvarTextEntry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/Controls/KeyToggleCheckButton.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui/Controls/LabeledCommandComboBox.cpp"
    "${VGUI}/controls_link_check.cpp"
    "${SDK}/public/UnicodeFileHelpers.cpp"
    ${_vgui_core}
    ${_vgui_controls}
    ${_tier1}
)

target_include_directories(csretro_menu PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/gameui"
    "${VGUI}"
    "${SHIM}"
    "${SHIMCASE}"
    "${CSRETRO_ROOT}/engine/common"
    "${CSRETRO_ROOT}/engine/public"
    "${CSRETRO_ROOT}/engine/engine"
    "${BODY}/common"
    "${BODY}/public"
    "${BODY}/public/cl_dll"
    "${BODY}/dlls"
    "${BODY}/engine"
    "${SDK}/public"
    "${SDK}/public/tier0"
    "${SDK}/public/tier1"
    "${SDK}/public/vgui"
    "${SDK}/common"
    "${SDK}/engine"
    "${SDK}/vgui2/src"
    "${SDK}/vgui2"
    "${SDK}"
    ${FREETYPE_INCLUDE_DIRS}
)

target_compile_features(csretro_menu PRIVATE cxx_std_17)

target_compile_definitions(csretro_menu PRIVATE
    GNUC
    POSIX
    LINUX
    _LINUX
    NDEBUG
    NO_MALLOC_OVERRIDE
    STATIC_TIER0
    NO_VCR
    NO_STEAM
    MATHLIB_HEADER_ONLY
    VALVE_LITTLE_ENDIAN
    PLATFORM_64BITS
    _DLL_EXT=".so"
    SOURCE_SDK_GFX_PATH="gfx/vgui2"
    stricmp=strcasecmp
    strnicmp=strncasecmp
)

target_compile_options(csretro_menu PRIVATE
    -fno-strict-aliasing
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-ignored-attributes
    -Wno-invalid-offsetof
    -Wno-class-memaccess
    -Wno-sign-compare
    -Wno-reorder
    -Wno-misleading-indentation
    ${FREETYPE_CFLAGS_OTHER}
)

target_link_libraries(csretro_menu PRIVATE ${FREETYPE_LIBRARIES} dl pthread)

if(NOT APPLE)
    if(NOT CSRETRO_MENU_SANITIZE)
        target_link_options(csretro_menu PRIVATE -Wl,--no-undefined)
    endif()
endif()

if(CSRETRO_MENU_SANITIZE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "CSRETRO_MENU_SANITIZE braucht Clang oder GCC.")
    endif()
    target_compile_options(csretro_menu PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(csretro_menu PRIVATE -fsanitize=address,undefined)
    message(STATUS "Menü Sanitizer: ASan+UBSan")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(_menu_postfix "_arm64")
else()
    set(_menu_postfix "_amd64")
endif()

set_target_properties(csretro_menu PROPERTIES
    OUTPUT_NAME "menu${_menu_postfix}"
    PREFIX ""
    CXX_VISIBILITY_PRESET default
    C_VISIBILITY_PRESET default
)

if(UNIX AND NOT APPLE)
    set_target_properties(csretro_menu PROPERTIES SUFFIX ".so")
endif()
