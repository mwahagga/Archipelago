#include "shell_command.h"
#include "../pipe_server.h"
#include <MinHook.h>
#include <cstdio>

namespace Halo3AP {

    std::atomic<bool> g_quitAfterComplete{false};

    namespace {
        // IGameEngine vtable slot 3 signature:
        // void ShellCommand(void* engineObj, int msgType, void* context)
        typedef void (*ShellCommandFn)(void* engineObj, int msgType, void* context);
        ShellCommandFn g_originalShellCommand = nullptr;
        void* g_shellCommandTarget = nullptr;
        void** g_engineGlobal = nullptr;

        void DetourShellCommand(void* engineObj, int msgType, void* context) {
            // When game_won occurred, intercept whatever command was queued (e.g. next mission)
            // and dispatch the clean engine quit sequence: pause (0) -> teardown (0xD) -> resume (1)
            if (g_quitAfterComplete.load()) {
                g_quitAfterComplete.store(false);
                Log("[ShellCommand] Intercepting command 0x%X - sending clean teardown to menu...", msgType);

                if (g_originalShellCommand) {
                    g_originalShellCommand(engineObj, 0x0, nullptr); // Pause engine
                    g_originalShellCommand(engineObj, 0xD, nullptr); // Teardown / Return to menu
                    g_originalShellCommand(engineObj, 0x1, nullptr); // Resume loop
                }
                return;
            }

            if (g_originalShellCommand) {
                g_originalShellCommand(engineObj, msgType, context);
            }
        }
    } // anonymous namespace

    void* GetEngineObject() {
        if (!g_engineGlobal) return nullptr;
        return *g_engineGlobal;
    }

    bool IsShellCommandHookActive() {
        return g_shellCommandTarget != nullptr;
    }

    bool InstallShellCommandHook(HMODULE hHalo3) {
        if (g_shellCommandTarget) {
            return true;
        }

        if (!hHalo3) {
            Log("[ShellCommand] halo3.dll handle is null");
            return false;
        }

        // Dynamically find CreateGameEngine export
        auto createEngine = (uint8_t*)GetProcAddress(hHalo3, "CreateGameEngine");
        if (!createEngine) {
            Log("[ShellCommand] CreateGameEngine export not found in halo3.dll");
            return false;
        }

        // Scan for MOV [rip + disp32], reg64 storing the engine pointer (up to 1024 bytes)
        void** found = nullptr;
        for (int i = 0; i < 1024; ++i) {
            if (createEngine[i] == 0x48 &&
                createEngine[i + 1] == 0x89 &&
                (createEngine[i + 2] & 0xC7) == 0x05) {
                int32_t disp = *(int32_t*)(&createEngine[i + 3]);
                uint8_t* nextInstr = &createEngine[i + 7];
                found = (void**)(nextInstr + disp);
                break;
            }
        }

        if (!found) {
            // Fallback to verified RVA +0xA3CA90
            found = (void**)((uint8_t*)hHalo3 + 0xA3CA90);
            Log("[ShellCommand] Using fallback engine global pointer at halo3.dll + 0xA3CA90");
        } else {
            uintptr_t rva = (uintptr_t)found - (uintptr_t)hHalo3;
            Log("[ShellCommand] Resolved engine global pointer at halo3.dll + 0x%llX", (unsigned long long)rva);
        }

        g_engineGlobal = found;
        void* engineObj = *g_engineGlobal;
        if (!engineObj) {
            Log("[ShellCommand] Engine object not yet created by MCC");
            return false;
        }

        void** vtable = *(void***)engineObj;
        if (!vtable) {
            Log("[ShellCommand] Engine vtable is null");
            return false;
        }

        // Vtable slot 3 = ShellCommand
        void* slot3 = vtable[3];
        uintptr_t slot3Rva = (uintptr_t)slot3 - (uintptr_t)hHalo3;
        Log("[ShellCommand] Targeting vtable[3] at halo3.dll + 0x%llX", (unsigned long long)slot3Rva);

        MH_STATUS status = MH_CreateHook(slot3, (void*)&DetourShellCommand, (void**)&g_originalShellCommand);
        if (status != MH_OK) {
            Log("[ShellCommand] MH_CreateHook failed: %d", status);
            return false;
        }

        status = MH_EnableHook(slot3);
        if (status != MH_OK) {
            Log("[ShellCommand] MH_EnableHook failed: %d", status);
            MH_RemoveHook(slot3);
            return false;
        }

        g_shellCommandTarget = slot3;
        Log("[ShellCommand] Hook active!");
        return true;
    }

    void UninstallShellCommandHook() {
        if (g_shellCommandTarget) {
            MH_DisableHook(g_shellCommandTarget);
            MH_RemoveHook(g_shellCommandTarget);
            g_shellCommandTarget = nullptr;
        }
        g_originalShellCommand = nullptr;
        g_engineGlobal = nullptr;
        g_quitAfterComplete.store(false);
    }

} // namespace Halo3AP
