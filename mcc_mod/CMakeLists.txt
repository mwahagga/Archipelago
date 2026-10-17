cmake_minimum_required(VERSION 3.15)
project(Halo3ArchipelagoMod LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dxgi SHARED
    proxy_dxgi.cpp
    pipe_server.cpp
    hook_manager.cpp
    mod_main.cpp
    engine/tag_system.cpp
    engine/object_table.cpp
    hooks/lobby_hooks.cpp
    hooks/weapon_hooks.cpp
    hooks/engine_hooks.cpp
    hooks/event_hooks.cpp
    hooks/shell_command.cpp
    hooks/menu_hooks.cpp
    hooks/vfs_hooks.cpp
    pattern_scan.cpp
    third_party/minhook/src/hook.c
    third_party/minhook/src/buffer.c
    third_party/minhook/src/trampoline.c
    third_party/minhook/src/hde/hde64.c
)

target_include_directories(dxgi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/include
)

target_link_libraries(dxgi PRIVATE
    ole32
    ws2_32
)

# Output dxgi.dll without prefix (not libdxgi.dll)
set_target_properties(dxgi PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dxgi"
)

if(MSVC)
    target_compile_options(dxgi PRIVATE /W3 /O2)
else()
    target_compile_options(dxgi PRIVATE -O2 -Wall)
    target_link_options(dxgi PRIVATE -static -static-libgcc -static-libstdc++)
endif()
