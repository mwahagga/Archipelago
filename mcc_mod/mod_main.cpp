#include "pipe_server.h"
#include <windows.h>
#include "hook_manager.h"

// Forward declaration from proxy_dxgi.cpp
bool LoadRealDXGI();
void FreeRealDXGI();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);

        // Pin the DLL so MCC cannot unload it during map transitions / device reset
        HMODULE hSelf = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCSTR)DllMain,
            &hSelf
        );

        Halo3AP::Log("========================================");
        Halo3AP::Log(" Halo 3 Archipelago Native Mod Initialized (DXGI)");
        Halo3AP::Log("========================================");

        // Check command line for -eac-nop-loaded or -no-eac (Anti-Cheat Disabled mode).
        // If MCC is launched with Anti-Cheat ENABLED, we MUST remain completely inert
        // as a passthrough to prevent any interference with EasyAntiCheat.
        const char* cmdLine = GetCommandLineA();
        bool eacDisabled = (cmdLine && (strstr(cmdLine, "-eac-nop-loaded") != nullptr || strstr(cmdLine, "-no-eac") != nullptr));

        // 1. Initialize DXGI proxy to system DLL
        if (!LoadRealDXGI()) {
            Halo3AP::Log("[!] Failed to load system dxgi.dll!");
            return FALSE;
        }
        Halo3AP::Log("[+] System dxgi.dll proxy initialized.");

        if (!eacDisabled) {
            Halo3AP::Log("==================================================================");
            Halo3AP::Log("[SAFETY] MCC was launched WITHOUT -eac-nop-loaded!");
            Halo3AP::Log("[SAFETY] Anti-Cheat is active! Mod is entering INERT PASSTHROUGH mode.");
            Halo3AP::Log("[SAFETY] No hooks installed, no IPC pipe created. 0 ban risk.");
            Halo3AP::Log("==================================================================");
            return TRUE;
        }

        // 2. Install in-engine level gating hooks
        if (Halo3AP::InstallHooks()) {
            Halo3AP::Log("[+] In-engine level gating hooks active.");
        } else {
            Halo3AP::Log("[!] Failed to install level gating hooks.");
        }

        // 3. Start IPC threads: Named Pipe (Windows) and TCP (Universal Windows / Linux Proton)
        HANDLE hPipeThread = CreateThread(nullptr, 0, Halo3AP::PipeServerThread, nullptr, 0, nullptr);
        if (hPipeThread) {
            CloseHandle(hPipeThread);
        }
        HANDLE hTcpThread = CreateThread(nullptr, 0, Halo3AP::TcpServerThread, nullptr, 0, nullptr);
        if (hTcpThread) {
            CloseHandle(hTcpThread);
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        Halo3AP::StopMonitor();
        if (lpReserved == nullptr) {
            Halo3AP::Log("[Halo3AP Mod] FreeLibrary called on dxgi.dll, but mod is pinned! Preserving hooks.");
            break;
        }
        Halo3AP::Log("[Halo3AP Mod] Process terminating cleanly.");
        // During process exit (lpReserved != nullptr), do not invoke complex unhooking or FreeLibrary;
        // kernel teardown reclaims all resources safely without risking access violations in unmapped modules.
        break;
    }
    return TRUE;
}

