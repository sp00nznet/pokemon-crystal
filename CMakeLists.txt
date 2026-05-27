cmake_minimum_required(VERSION 3.16)
project(rom C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# The gb-recompiled runtime uses C++20 features (designated initialisers,
# concepts), so this is required when building under MSVC. LinksAwakening
# happens to get away without it because MinGW defaults higher.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3)
endif()

# Multiplayer overlay (from sp00nznet/la-mp, embedded in the gb-recompiled
# runtime). The runtime currently calls mp_session_init / mp_session_update
# / mp_menu_init / mp_indicators_render unconditionally from
# platform_sdl.cpp and menu_gui.cpp — so the overlay is effectively part
# of the runtime. Turning this OFF requires upstream patches in our
# gb-recompiled fork to gate those calls.
#
# For Pokemon Crystal this is built but unused unless/until we wire the
# overlay up to Gen 2 link-cable trades/battles in a later milestone.
option(GAME_MULTIPLAYER "Build with ENet + multiplayer overlay" ON)

# Runtime lives in the gbrecomp/ submodule (sp00nznet/gb-recompiled).
set(GBRT_DIR "${CMAKE_SOURCE_DIR}/gbrecomp/runtime")

if(NOT EXISTS "${GBRT_DIR}/include/platform_sdl.h")
    message(FATAL_ERROR
        "gbrecomp/ submodule is empty. Run:\n"
        "  git submodule update --init --recursive")
endif()

find_package(SDL2 REQUIRED)

add_library(gbrt STATIC
    ${GBRT_DIR}/src/gbrt.c
    ${GBRT_DIR}/src/ppu.c
    ${GBRT_DIR}/src/audio.c
    ${GBRT_DIR}/src/interpreter.c
    ${GBRT_DIR}/src/hwtrace.c
    ${GBRT_DIR}/src/platform_sdl.cpp
    ${GBRT_DIR}/src/menu_gui.cpp
    ${GBRT_DIR}/src/asset_viewer.cpp
)

target_sources(gbrt PRIVATE
    ${GBRT_DIR}/third_party/imgui/imgui.cpp
    ${GBRT_DIR}/third_party/imgui/imgui_draw.cpp
    ${GBRT_DIR}/third_party/imgui/imgui_tables.cpp
    ${GBRT_DIR}/third_party/imgui/imgui_widgets.cpp
    ${GBRT_DIR}/third_party/imgui/imgui_demo.cpp
    ${GBRT_DIR}/third_party/imgui/backends/imgui_impl_sdl2.cpp
    ${GBRT_DIR}/third_party/imgui/backends/imgui_impl_sdlrenderer2.cpp
)

if(GAME_MULTIPLAYER)
    target_sources(gbrt PRIVATE
        ${GBRT_DIR}/third_party/enet/callbacks.c
        ${GBRT_DIR}/third_party/enet/compress.c
        ${GBRT_DIR}/third_party/enet/host.c
        ${GBRT_DIR}/third_party/enet/list.c
        ${GBRT_DIR}/third_party/enet/packet.c
        ${GBRT_DIR}/third_party/enet/peer.c
        ${GBRT_DIR}/third_party/enet/protocol.c
        ${GBRT_DIR}/third_party/enet/win32.c

        ${GBRT_DIR}/src/multiplayer/mp_net.cpp
        ${GBRT_DIR}/src/multiplayer/mp_session.cpp
        ${GBRT_DIR}/src/multiplayer/mp_menu.cpp
        ${GBRT_DIR}/src/multiplayer/mp_color.cpp
        ${GBRT_DIR}/src/multiplayer/mp_worldsync.cpp
        ${GBRT_DIR}/src/multiplayer/mp_indicators.cpp
        ${GBRT_DIR}/src/multiplayer/mp_delta.cpp
        ${GBRT_DIR}/src/multiplayer/mp_trade.cpp
        ${GBRT_DIR}/src/multiplayer/mp_voice.cpp
        ${GBRT_DIR}/src/multiplayer/mp_pvp.cpp
        ${GBRT_DIR}/src/multiplayer/mp_sprites.cpp
        ${GBRT_DIR}/src/multiplayer/mp_gen2_serial.cpp
    )
    target_compile_definitions(gbrt PUBLIC LA_HAS_MULTIPLAYER)
endif()

target_include_directories(gbrt PUBLIC
    ${GBRT_DIR}/include
    ${GBRT_DIR}/third_party/imgui
    ${GBRT_DIR}/third_party/enet/include
    ${GBRT_DIR}/src
)

target_link_libraries(gbrt PUBLIC SDL2::SDL2)
if(WIN32 AND GAME_MULTIPLAYER)
    target_link_libraries(gbrt PUBLIC ws2_32 winmm)
endif()
target_compile_definitions(gbrt PUBLIC GB_HAS_SDL2)
# We list menu_gui.cpp and asset_viewer.cpp directly (rather than going
# through the runtime's own CMakeLists which gates them on LA_HAS_IMGUI
# and supplies stubs when off). They unconditionally call ImGui APIs,
# so the matching context setup in platform_sdl.cpp must run too —
# enable LA_HAS_IMGUI here. Without this, menu_gui_init crashes during
# gb_platform_init.
target_compile_definitions(gbrt PUBLIC LA_HAS_IMGUI)

# When linking the recompiled rom.c (which always provides gb_dispatch /
# gb_dispatch_call), compile out the weak fallbacks in gbrt.c. Required
# under MSVC, which doesn't honour `__attribute__((weak))` — the fallback
# would otherwise duplicate the rom.c definitions at link time. Safe
# (and slightly cleaner) on every other compiler too.
target_compile_definitions(gbrt PUBLIC GB_RECOMPILED_DISPATCH)

# Brand the window. The runtime defaults to "Game Boy Recompiled" if no
# project sets GBRT_WINDOW_TITLE.
target_compile_definitions(gbrt PUBLIC GBRT_WINDOW_TITLE="Pokemon Crystal Recompiled")

# rom.c and rom_rom.c are recompiler output. They are gitignored. Generate
# them locally by running gbrecomp/recompiler/ against your own legally
# obtained Pokemon Crystal ROM.
add_executable(rom
    rom_main.c
    rom.c
    rom_rom.c
)

target_link_libraries(rom gbrt)
