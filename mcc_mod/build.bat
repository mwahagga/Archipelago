@echo off
setlocal
echo ===================================================
echo  Compiling Halo 3 Archipelago Native Mod (dxgi.dll)
echo  Using MinHook for safe x64 function hooking
echo ===================================================

set CXX=C:\Strawberry\c\bin\g++.exe

if not exist "%CXX%" (
    echo [ERROR] MinGW G++ compiler not found at %CXX%!
    exit /b 1
)

echo Using compiler: %CXX%

"%CXX%" -shared -O2 -std=c++17 ^
    -static -static-libgcc -static-libstdc++ ^
    -Ithird_party/minhook/include ^
    -I. ^
    proxy_dxgi.cpp pipe_server.cpp hook_manager.cpp mod_main.cpp ^
    engine/tag_system.cpp engine/object_table.cpp ^
    hooks/lobby_hooks.cpp hooks/weapon_hooks.cpp hooks/engine_hooks.cpp hooks/event_hooks.cpp ^
    third_party/minhook/src/hook.c third_party/minhook/src/buffer.c third_party/minhook/src/trampoline.c ^
    third_party/minhook/src/hde/hde64.c ^
    -o dxgi.dll ^
    -lole32 -lws2_32

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo  [SUCCESS] dxgi.dll successfully compiled!
echo ===================================================

set BUNDLE_DEST=..\worlds\halo3\mod
if not exist "%BUNDLE_DEST%" mkdir "%BUNDLE_DEST%"
copy /Y dxgi.dll "%BUNDLE_DEST%\dxgi.dll" >nul
if %ERRORLEVEL% EQU 0 (
    echo [BUNDLE] Successfully updated worlds\halo3\mod\dxgi.dll!
) else (
    echo [WARNING] Failed to copy dxgi.dll to worlds\halo3\mod\
)

