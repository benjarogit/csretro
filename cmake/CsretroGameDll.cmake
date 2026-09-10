# Eine 64-Bit-GameDLL. Upstream-CMake/SLN werden nicht aufgerufen.
# Quellen: server/game/ATTRIBUTION.md

include("${CMAKE_CURRENT_LIST_DIR}/CsretroLibraryNaming.cmake")

option(CSRETRO_BUILD_GAMEDLL "CS-Retro-GameDLL (ReGameDLL-Körper) gegen Xash" ON)
option(CSRETRO_GAMEDLL_SANITIZE "ASan+UBSan (Entwicklungsbuild)" OFF)

if(NOT CSRETRO_BUILD_GAMEDLL)
    return()
endif()

set(G "${CSRETRO_ROOT}/server/game/regamedll")

set(CSRETRO_GAMEDLL_SOURCES
    "${G}/engine/unicode_strtools.cpp"
    "${G}/game_shared/shared_util.cpp"
    "${G}/game_shared/voice_gamemgr.cpp"
    "${G}/game_shared/bot/bot.cpp"
    "${G}/game_shared/bot/bot_manager.cpp"
    "${G}/game_shared/bot/bot_profile.cpp"
    "${G}/game_shared/bot/bot_util.cpp"
    "${G}/game_shared/bot/nav_area.cpp"
    "${G}/game_shared/bot/nav_file.cpp"
    "${G}/game_shared/bot/nav_node.cpp"
    "${G}/game_shared/bot/nav_path.cpp"
    "${G}/pm_shared/pm_debug.cpp"
    "${G}/pm_shared/pm_math.cpp"
    "${G}/pm_shared/pm_shared.cpp"
    "${G}/regamedll/regamedll.cpp"
    "${G}/regamedll/precompiled.cpp"
    "${G}/regamedll/public_amalgamation.cpp"
    "${G}/regamedll/hookchains_impl.cpp"
    "${G}/public/FileSystem.cpp"
    "${G}/public/interface.cpp"
    "${G}/public/MemPool.cpp"
    "${G}/public/tier0/dbg.cpp"
    "${G}/dlls/airtank.cpp"
    "${G}/dlls/ammo.cpp"
    "${G}/dlls/animating.cpp"
    "${G}/dlls/animation.cpp"
    "${G}/dlls/basemonster.cpp"
    "${G}/dlls/bmodels.cpp"
    "${G}/dlls/buttons.cpp"
    "${G}/dlls/career_tasks.cpp"
    "${G}/dlls/cbase.cpp"
    "${G}/dlls/client.cpp"
    "${G}/dlls/cmdhandler.cpp"
    "${G}/dlls/combat.cpp"
    "${G}/dlls/debug.cpp"
    "${G}/dlls/doors.cpp"
    "${G}/dlls/effects.cpp"
    "${G}/dlls/explode.cpp"
    "${G}/dlls/func_break.cpp"
    "${G}/dlls/func_tank.cpp"
    "${G}/dlls/game.cpp"
    "${G}/dlls/gamerules.cpp"
    "${G}/dlls/ggrenade.cpp"
    "${G}/dlls/inferno.cpp"
    "${G}/dlls/gib.cpp"
    "${G}/dlls/globals.cpp"
    "${G}/dlls/h_battery.cpp"
    "${G}/dlls/h_cycler.cpp"
    "${G}/dlls/h_export.cpp"
    "${G}/dlls/healthkit.cpp"
    "${G}/dlls/hintmessage.cpp"
    "${G}/dlls/items.cpp"
    "${G}/dlls/lights.cpp"
    "${G}/dlls/mapinfo.cpp"
    "${G}/dlls/maprules.cpp"
    "${G}/dlls/mortar.cpp"
    "${G}/dlls/multiplay_gamerules.cpp"
    "${G}/dlls/observer.cpp"
    "${G}/dlls/pathcorner.cpp"
    "${G}/dlls/plats.cpp"
    "${G}/dlls/player.cpp"
    "${G}/dlls/revert_saved.cpp"
    "${G}/dlls/saverestore.cpp"
    "${G}/dlls/singleplay_gamerules.cpp"
    "${G}/dlls/skill.cpp"
    "${G}/dlls/sound.cpp"
    "${G}/dlls/soundent.cpp"
    "${G}/dlls/spectator.cpp"
    "${G}/dlls/subs.cpp"
    "${G}/dlls/training_gamerules.cpp"
    "${G}/dlls/triggers.cpp"
    "${G}/dlls/tutor.cpp"
    "${G}/dlls/tutor_base_states.cpp"
    "${G}/dlls/tutor_base_tutor.cpp"
    "${G}/dlls/tutor_cs_states.cpp"
    "${G}/dlls/tutor_cs_tutor.cpp"
    "${G}/dlls/util.cpp"
    "${G}/dlls/vehicle.cpp"
    "${G}/dlls/weapons.cpp"
    "${G}/dlls/weapontype.cpp"
    "${G}/dlls/world.cpp"
    "${G}/dlls/API/CAPI_Impl.cpp"
    "${G}/dlls/API/CSEntity.cpp"
    "${G}/dlls/API/CSPlayer.cpp"
    "${G}/dlls/API/CSPlayerItem.cpp"
    "${G}/dlls/API/CSPlayerWeapon.cpp"
    "${G}/dlls/addons/item_airbox.cpp"
    "${G}/dlls/addons/point_command.cpp"
    "${G}/dlls/addons/trigger_random.cpp"
    "${G}/dlls/addons/trigger_setorigin.cpp"
    "${G}/dlls/wpn_shared/wpn_ak47.cpp"
    "${G}/dlls/wpn_shared/wpn_aug.cpp"
    "${G}/dlls/wpn_shared/wpn_awp.cpp"
    "${G}/dlls/wpn_shared/wpn_c4.cpp"
    "${G}/dlls/wpn_shared/wpn_deagle.cpp"
    "${G}/dlls/wpn_shared/wpn_elite.cpp"
    "${G}/dlls/wpn_shared/wpn_famas.cpp"
    "${G}/dlls/wpn_shared/wpn_fiveseven.cpp"
    "${G}/dlls/wpn_shared/wpn_flashbang.cpp"
    "${G}/dlls/wpn_shared/wpn_g3sg1.cpp"
    "${G}/dlls/wpn_shared/wpn_galil.cpp"
    "${G}/dlls/wpn_shared/wpn_glock18.cpp"
    "${G}/dlls/wpn_shared/wpn_hegrenade.cpp"
    "${G}/dlls/wpn_shared/wpn_incgrenade.cpp"
    "${G}/dlls/wpn_shared/wpn_molotov.cpp"
    "${G}/dlls/wpn_shared/wpn_knife.cpp"
    "${G}/dlls/wpn_shared/wpn_m3.cpp"
    "${G}/dlls/wpn_shared/wpn_m4a1.cpp"
    "${G}/dlls/wpn_shared/wpn_m249.cpp"
    "${G}/dlls/wpn_shared/wpn_mac10.cpp"
    "${G}/dlls/wpn_shared/wpn_mp5navy.cpp"
    "${G}/dlls/wpn_shared/wpn_p90.cpp"
    "${G}/dlls/wpn_shared/wpn_p228.cpp"
    "${G}/dlls/wpn_shared/wpn_scout.cpp"
    "${G}/dlls/wpn_shared/wpn_sg550.cpp"
    "${G}/dlls/wpn_shared/wpn_sg552.cpp"
    "${G}/dlls/wpn_shared/wpn_smokegrenade.cpp"
    "${G}/dlls/wpn_shared/wpn_tmp.cpp"
    "${G}/dlls/wpn_shared/wpn_ump45.cpp"
    "${G}/dlls/wpn_shared/wpn_usp.cpp"
    "${G}/dlls/wpn_shared/wpn_xm1014.cpp"
    "${G}/dlls/bot/cs_bot.cpp"
    "${G}/dlls/bot/cs_bot_chatter.cpp"
    "${G}/dlls/bot/cs_bot_event.cpp"
    "${G}/dlls/bot/cs_bot_init.cpp"
    "${G}/dlls/bot/cs_bot_learn.cpp"
    "${G}/dlls/bot/cs_bot_listen.cpp"
    "${G}/dlls/bot/cs_bot_manager.cpp"
    "${G}/dlls/bot/cs_bot_nav.cpp"
    "${G}/dlls/bot/cs_bot_pathfind.cpp"
    "${G}/dlls/bot/cs_bot_radio.cpp"
    "${G}/dlls/bot/cs_bot_statemachine.cpp"
    "${G}/dlls/bot/cs_bot_update.cpp"
    "${G}/dlls/bot/cs_bot_vision.cpp"
    "${G}/dlls/bot/cs_bot_weapon.cpp"
    "${G}/dlls/bot/cs_gamestate.cpp"
    "${G}/dlls/bot/states/cs_bot_attack.cpp"
    "${G}/dlls/bot/states/cs_bot_buy.cpp"
    "${G}/dlls/bot/states/cs_bot_defuse_bomb.cpp"
    "${G}/dlls/bot/states/cs_bot_escape_from_bomb.cpp"
    "${G}/dlls/bot/states/cs_bot_fetch_bomb.cpp"
    "${G}/dlls/bot/states/cs_bot_follow.cpp"
    "${G}/dlls/bot/states/cs_bot_hide.cpp"
    "${G}/dlls/bot/states/cs_bot_hunt.cpp"
    "${G}/dlls/bot/states/cs_bot_idle.cpp"
    "${G}/dlls/bot/states/cs_bot_investigate_noise.cpp"
    "${G}/dlls/bot/states/cs_bot_move_to.cpp"
    "${G}/dlls/bot/states/cs_bot_plant_bomb.cpp"
    "${G}/dlls/bot/states/cs_bot_use_entity.cpp"
    "${G}/dlls/hostage/hostage.cpp"
    "${G}/dlls/hostage/hostage_improv.cpp"
    "${G}/dlls/hostage/hostage_localnav.cpp"
    "${G}/dlls/hostage/states/hostage_animate.cpp"
    "${G}/dlls/hostage/states/hostage_escape.cpp"
    "${G}/dlls/hostage/states/hostage_follow.cpp"
    "${G}/dlls/hostage/states/hostage_idle.cpp"
    "${G}/dlls/hostage/states/hostage_retreat.cpp"
)

if(WIN32)
    list(APPEND CSRETRO_GAMEDLL_SOURCES "${G}/public/tier0/platform_win32.cpp")
else()
    list(APPEND CSRETRO_GAMEDLL_SOURCES "${G}/public/tier0/platform_posix.cpp")
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    list(APPEND CSRETRO_GAMEDLL_SOURCES "${G}/regamedll/sse_mathfun.cpp")
endif()

add_library(csretro_gamedll SHARED ${CSRETRO_GAMEDLL_SOURCES})

target_include_directories(csretro_gamedll PRIVATE
    "${G}"
    "${G}/engine"
    "${G}/common"
    "${G}/dlls"
    "${G}/game_shared"
    "${G}/pm_shared"
    "${G}/public"
    "${G}/public/regamedll"
    "${G}/regamedll"
)

# GoldSrc/ReGameDLL-Defines. Kein _GLIBCXX_USE_CXX11_ABI=0 (32-Bit-ReHLDS-Rest).
# XASH_64BIT kommt aus public/build.h bei LP64.
target_compile_definitions(csretro_gamedll PRIVATE
    REGAMEDLL_FIXES
    REGAMEDLL_API
    REGAMEDLL_ADD
    UNICODE_FIXES
    BUILD_LATEST
    CLIENT_WEAPONS
    USE_QSTRING
    XASH_64BIT
    _stricmp=strcasecmp
    _strnicmp=strncasecmp
    _strdup=strdup
    _unlink=unlink
    _snprintf=snprintf
    _vsnprintf=vsnprintf
)

if(NOT WIN32)
    target_compile_definitions(csretro_gamedll PRIVATE
        LINUX
        _LINUX
        _write=write
        _close=close
        _access=access
        _open=open
        _vsnwprintf=vswprintf
    )
endif()

if(NOT MSVC)
    target_compile_options(csretro_gamedll PRIVATE
        -fno-strict-aliasing
        -fno-exceptions
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-unused-private-field
        -Wno-unused-but-set-variable
        -Wno-overloaded-virtual
        -Wno-invalid-offsetof
        -Wno-sign-compare
        -Wno-switch
        -Wno-format
        -Wno-unknown-pragmas
        -Wno-write-strings
        -fpermissive
        -fno-sized-deallocation
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        target_compile_options(csretro_gamedll PRIVATE -msse3)
    endif()
endif()

if(UNIX)
    target_link_libraries(csretro_gamedll PRIVATE m dl)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(csretro_gamedll PRIVATE
        -Wl,-z,notext
        "-Wl,--version-script=${CSRETRO_ROOT}/server/game/version_script.lds"
    )
    # ASan/UBSan-Symbole kommen vom Runtime (LD_PRELOAD / Host). --no-undefined würde den Link sprengen.
    if(NOT CSRETRO_GAMEDLL_SANITIZE)
        target_link_options(csretro_gamedll PRIVATE -Wl,--no-undefined)
    endif()
endif()

if(CSRETRO_GAMEDLL_SANITIZE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "CSRETRO_GAMEDLL_SANITIZE braucht Clang oder GCC.")
    endif()
    target_compile_options(csretro_gamedll PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(csretro_gamedll PRIVATE -fsanitize=address,undefined)
    message(STATUS "GameDLL Sanitizer: ASan+UBSan")
endif()

csretro_xash_lib_name(_csretro_cs_name "cs")
set_target_properties(csretro_gamedll PROPERTIES
    OUTPUT_NAME "${_csretro_cs_name}"
    PREFIX ""
    CXX_VISIBILITY_PRESET default
    C_VISIBILITY_PRESET default
)

message(STATUS "  GameDLL: ON → ${_csretro_cs_name}${CMAKE_SHARED_LIBRARY_SUFFIX}")
