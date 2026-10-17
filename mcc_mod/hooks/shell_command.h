#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atomic>

namespace Halo3AP {

    // Flag set when a mission is completed, signaling ShellCommand to return to menu
    extern std::atomic<bool> g_quitAfterComplete;

    // Returns pointer to the current IGameEngine instance
    void* GetEngineObject();

    // Returns true if the hook on ShellCommand is currently active
    bool IsShellCommandHookActive();

    // Hooks vtable[3] (ShellCommand) on halo3.dll's IGameEngine
    bool InstallShellCommandHook(HMODULE hHalo3);

    // Unhooks ShellCommand
    void UninstallShellCommandHook();

} // namespace Halo3AP
