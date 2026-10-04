#include "hook_manager.h"
#include "pipe_server.h"
#include "offsets.h"
#include "memory_utils.h"
#include "hooks/lobby_hooks.h"
#include "hooks/weapon_hooks.h"
#include "hooks/engine_hooks.h"
#include "hooks/event_hooks.h"
#include "engine/tag_system.h"
#include "engine/object_table.h"

#include <MinHook.h>

#include <atomic>

// Detailed reverse-engineering documentation, failed hook postmortems,
// and memory struct layouts have been moved to:
//   mcc_mod/docs/reverse_engineering_notes.md
//
// All hook and struct memory offsets are centralized in:
//   mcc_mod/offsets.h

namespace Halo3AP {

// ============================================================================
// SHARED STATE — owned by hook_manager, accessed by hook modules via extern
// ============================================================================

uintptr_t g_halo3Base = 0;
bool      g_halo3HooksInstalled = false;
bool      g_mapScanned = false;
static std::atomic<bool> g_stopMonitor{false};

// ============================================================================
// DATA-DRIVEN HOOK TABLE
//
// Each hook is described as a table entry. InstallHalo3Hooks iterates the
// table to create, enable, and log each hook. RemoveHalo3Hooks iterates
// to disable and remove. Adding a new hook is a one-line table entry.
// ============================================================================

struct HookEntry {
    const char* name;           // Human-readable name for logging
    uintptr_t   offset;         // Offset from halo3.dll base
    void*       detourFn;       // Pointer to detour function
    void**      originalFnPtr;  // Where MinHook stores the trampoline
    void**      targetPtr;      // Where we store the computed target address
};

static const HookEntry g_halo3Hooks[] = {
    {"raycast_evaluate_candidate",   Halo3Offsets::H3_RAYCAST_EVALUATE_CANDIDATE,
     (void*)&Detour_RaycastEvaluateCandidate,  (void**)&g_originalRaycastEvaluateCandidate,  &g_targetRaycastEvaluateCandidate},

    {"device_evaluate_candidate",    Halo3Offsets::H3_DEVICE_EVALUATE_CANDIDATE,
     (void*)&Detour_DeviceEvaluateCandidate,   (void**)&g_originalDeviceEvaluateCandidate,   &g_targetDeviceEvaluateCandidate},

    {"unit_can_pickup_weapon",       Halo3Offsets::H3_UNIT_CAN_PICKUP_WEAPON,
     (void*)&Detour_unit_can_pickup_weapon,    (void**)&g_originalUnitCanPickupWeapon,       &g_targetUnitCanPickupWeapon},

    {"weapon_evaluate_candidate",    Halo3Offsets::H3_WEAPON_EVALUATE_CANDIDATE,
     (void*)&Detour_WeaponEvaluateCandidate,   (void**)&g_originalWeaponEvaluateCandidate,   &g_targetWeaponEvaluateCandidate},

    {"can_trade_with_ally",          Halo3Offsets::H3_CAN_TRADE_WITH_ALLY,
     (void*)&Detour_CanTradeWithAlly,          (void**)&g_originalCanTradeWithAlly,          &g_targetCanTradeWithAlly},

    {"unit_update_weapon_prompt",    Halo3Offsets::H3_UNIT_UPDATE_WEAPON_PROMPT,
     (void*)&Detour_unit_update_weapon_prompt, (void**)&g_originalUnitUpdateWeaponPrompt,    &g_targetUnitUpdateWeaponPrompt},

    {"unit_action_swap_weapon",      Halo3Offsets::H3_UNIT_ACTION_SWAP_WEAPON,
     (void*)&Detour_unit_action_swap_weapon,   (void**)&g_originalUnitActionSwapWeapon,      &g_targetUnitActionSwapWeapon},

    {"unit_action_pickup_weapon",    Halo3Offsets::H3_UNIT_ACTION_PICKUP_WEAPON,
     (void*)&Detour_unit_action_pickup_weapon, (void**)&g_originalUnitActionPickupWeapon,    &g_targetUnitActionPickupWeapon},

    {"unit_add_weapon",              Halo3Offsets::H3_UNIT_ADD_WEAPON,
     (void*)&Detour_unit_add_weapon,           (void**)&g_originalUnitAddWeapon,             &g_targetUnitAddWeapon},

    {"Halo3EngineStep",              Halo3Offsets::H3_ENGINE_STEP,
     (void*)&Detour_Halo3EngineStep,           (void**)&g_originalHalo3EngineStep,           &g_targetHalo3EngineStep},

    // Event detection hooks (rally points, terminals, skulls).
    {"game_insertion_point_unlock",   Halo3Offsets::H3_INSERTION_POINT_UNLOCK,
     (void*)&Detour_GameInsertionPointUnlock,   (void**)&g_originalInsertionPointUnlock,     &g_targetInsertionPointUnlock},

    {"terminal_activate",             Halo3Offsets::H3_TERMINAL_ACTIVATE,
     (void*)&Detour_TerminalActivate,          (void**)&g_originalTerminalActivate,          &g_targetTerminalActivate},

    {"award_primary_skull",           Halo3Offsets::H3_AWARD_PRIMARY_SKULL,
     (void*)&Detour_AwardPrimarySkull,         (void**)&g_originalAwardPrimarySkull,         &g_targetAwardPrimarySkull},

    {"award_secondary_skull",         Halo3Offsets::H3_AWARD_SECONDARY_SKULL,
     (void*)&Detour_AwardSecondarySkull,       (void**)&g_originalAwardSecondarySkull,       &g_targetAwardSecondarySkull},

    // Mission completion hook: intercepts game_won to auto-return to menu.
    {"game_won",                       Halo3Offsets::H3_GAME_WON,
     (void*)&Detour_GameWon,                   (void**)&g_originalGameWon,                   &g_targetGameWon},

    // Blocks campaign level advancement and directly triggers clean Save & Quit to MCC menu
    {"load_next_campaign_level",         Halo3Offsets::H3_LOAD_NEXT_CAMPAIGN_LEVEL,
     (void*)&Detour_LoadNextCampaignLevel,       (void**)&g_originalLoadNextCampaignLevel,       &g_targetLoadNextCampaignLevel},

    // Guards against crash at 0x355EB9 when unit's parent vehicle is despawned during outro cutscenes
    {"unit_get_seat_camera",             Halo3Offsets::H3_UNIT_GET_SEAT_CAMERA,
     (void*)&Detour_UnitGetSeatCamera,           (void**)&g_originalUnitGetSeatCamera,           &g_targetUnitGetSeatCamera},
};

static constexpr size_t NUM_HALO3_HOOKS = sizeof(g_halo3Hooks) / sizeof(g_halo3Hooks[0]);

// ============================================================================
// HOOK LIFECYCLE
// ============================================================================

void RemoveHalo3Hooks() {
    g_localPlayerDatum.store(0xFFFFFFFF);

    HMODULE hHalo3 = GetModuleHandleA("halo3.dll");
    bool halo3StillLoaded = (hHalo3 && (uintptr_t)hHalo3 == g_halo3Base);

    if (halo3StillLoaded && g_halo3Base) {
        uint8_t* pPatch = (uint8_t*)(g_halo3Base + Halo3Offsets::H3_GAME_WON_SKIP_NEXT_LEVEL_PATCH);
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(pPatch, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_NOACCESS)) {
            if (*pPatch == 0xEB && *(pPatch + 1) == 0x2A) {
                DWORD oldProtect = 0;
                if (VirtualProtect(pPatch, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    *pPatch = 0x75;
                    VirtualProtect(pPatch, 1, oldProtect, &oldProtect);
                    FlushInstructionCache(GetCurrentProcess(), pPatch, 1);
                }
            }
        }
        // Note: 0x12E5AB NOP patch removed - forcing the "campaign end" path caused black screen
        // because MCC doesn't handle campaign completion for non-final levels.

        uint8_t* pLoadNext = (uint8_t*)(g_halo3Base + Halo3Offsets::H3_MAIN_GAME_LOAD_NEXT_LEVEL);
        if (VirtualQuery(pLoadNext, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_NOACCESS)) {
            if (*pLoadNext == 0x30 && *(pLoadNext + 1) == 0xC0 && *(pLoadNext + 2) == 0xC3) {
                DWORD oldProtect = 0;
                if (VirtualProtect(pLoadNext, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    *pLoadNext = 0x40;
                    *(pLoadNext + 1) = 0x55;
                    *(pLoadNext + 2) = 0x48;
                    VirtualProtect(pLoadNext, 3, oldProtect, &oldProtect);
                    FlushInstructionCache(GetCurrentProcess(), pLoadNext, 3);
                }
            }
        }
    }

    for (size_t i = 0; i < NUM_HALO3_HOOKS; ++i) {
        if (*g_halo3Hooks[i].targetPtr) {
            if (halo3StillLoaded) {
                MH_DisableHook(*g_halo3Hooks[i].targetPtr);
                MH_RemoveHook(*g_halo3Hooks[i].targetPtr);
            }
            *g_halo3Hooks[i].targetPtr = nullptr;
        }
    }

    ResetTagRegistry();
    ResetEventState();
    g_halo3HooksInstalled = false;
}

bool InstallHalo3Hooks(uintptr_t halo3Base) {
    if (!halo3Base) return false;
    RemoveHalo3Hooks(); // Guarantee clean state before installing!

    g_halo3Base = halo3Base;
    g_mapScanned = false; // Reset so ScanAndRegisterMapWeapons runs on first simulation tick
    ResetEventState();
    Log("[Halo3AP Mod] Initializing halo3.dll hooks at base 0x%p...", (void*)halo3Base);

    // Patch 0xF05A3: change 'jne +0x2A' (0x75 0x2A) to 'jmp +0x2A' (0xEB 0x2A)
    // In game_won handler (0xF053C), this bypasses LoadNextCampaignLevel (+0x1398E4) and immediately
    // branches to the session conclusion / return to lobby routine (+0xF05CF).
    // This avoids hooking the global 0xF01A0 query (which has 150 callers across the engine and crashes
    // at 0x132140 if caller-saved registers like r9 are clobbered by detours).
    uint8_t* pPatch = (uint8_t*)(halo3Base + Halo3Offsets::H3_GAME_WON_SKIP_NEXT_LEVEL_PATCH);
    if (*pPatch == 0x75 && *(pPatch + 1) == 0x2A) {
        DWORD oldProtect = 0;
        if (VirtualProtect(pPatch, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *pPatch = 0xEB;
            VirtualProtect(pPatch, 1, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), pPatch, 1);
            Log("[+] Patched 0xF05A3 (0x75 -> 0xEB): game_won skips LoadNextCampaignLevel cleanly!");
        }
    }

    // Note: 0x12E5AB NOP patch removed. Forcing the "campaign end" path caused a black screen
    // because MCC's PGCR/menu transition doesn't work for non-final campaign levels.
    // Instead, we rely on the 0x12EBC4 patch below — when main_game_load_next_level returns false,
    // the engine falls through to its natural fallback at +0x12E619, which calls game state
    // recovery functions (0x180036EB0 with arg=2, 0x1802BDCBC, 0x18012FB1C) and cleanly resets.

    // Patch 0x12EBC4 (main_game_load_next_level): patch to 'xor al, al; ret' (0x30 0xC0 0xC3) as defense-in-depth.
    uint8_t* pLoadNext = (uint8_t*)(halo3Base + Halo3Offsets::H3_MAIN_GAME_LOAD_NEXT_LEVEL);
    if (*pLoadNext == 0x40 && *(pLoadNext + 1) == 0x55) {
        DWORD oldProtect = 0;
        if (VirtualProtect(pLoadNext, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *pLoadNext = 0x30;
            *(pLoadNext + 1) = 0xC0;
            *(pLoadNext + 2) = 0xC3;
            VirtualProtect(pLoadNext, 3, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), pLoadNext, 3);
            Log("[+] Patched 0x12EBC4 (main_game_load_next_level -> ret 0): extra safety against next level advance!");
        }
    }

    for (size_t i = 0; i < NUM_HALO3_HOOKS; ++i) {
        const HookEntry& h = g_halo3Hooks[i];
        *h.targetPtr = (void*)(halo3Base + h.offset);

        MH_STATUS status = MH_CreateHook(*h.targetPtr, h.detourFn, h.originalFnPtr);
        if (status == MH_OK) {
            MH_EnableHook(*h.targetPtr);
            Log("[+] %s hook active at halo3.dll + 0x%llX!", h.name, (unsigned long long)h.offset);
        } else {
            Log("[!] Failed to hook %s at +0x%llX: %s", h.name, (unsigned long long)h.offset, MH_StatusToString(status));
        }
    }

    g_halo3HooksInstalled = true;
    return true;
}

// Background watcher thread to dynamically hook halo3.dll when loaded
DWORD WINAPI Halo3MonitorThread(LPVOID lpParam) {
    Log("[Halo3AP Mod] halo3.dll monitor thread started.");
    while (!g_stopMonitor) {
        HMODULE hHalo3 = GetModuleHandleA("halo3.dll");
        if (hHalo3 && !g_stopMonitor) {
            uintptr_t halo3Base = (uintptr_t)hHalo3;
            // Verify PE header to make sure halo3.dll is fully loaded and valid
            IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)halo3Base;
            if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
                uint8_t firstByte = *(uint8_t*)(halo3Base + Halo3Offsets::H3_ENGINE_STEP);
                if (!g_halo3HooksInstalled || halo3Base != g_halo3Base || (firstByte != 0xE9 && firstByte != 0xFF)) {
                    Log("[Halo3AP Mod] Installing/re-installing halo3.dll hooks (base=0x%p, firstByte=0x%02X)...",
                        (void*)halo3Base, firstByte);
                    InstallHalo3Hooks(halo3Base);
                }
            }
        } else {
            if (g_halo3HooksInstalled) {
                Log("[Halo3AP Mod] halo3.dll unmapped/unloaded!");
                RemoveHalo3Hooks();
            }
            g_halo3HooksInstalled = false;
            g_halo3Base = 0;
        }

        Sleep(100);
    }
    Log("[Halo3AP Mod] halo3.dll monitor thread exiting cleanly.");
    return 0;
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool InstallHooks() {
    uintptr_t mccBase = (uintptr_t)GetModuleHandleA(nullptr);
    if (!mccBase) {
        Log("[!] Failed to get MCC base module handle!");
        return false;
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        Log("[!] MH_Initialize failed: %s", MH_StatusToString(status));
        return false;
    }

    // Hook CampaignLobby::StartCountdown at +0x5137EC
    g_targetStartCountdown = (void*)(mccBase + Halo3Offsets::MCC_START_COUNTDOWN);
    status = MH_CreateHook(g_targetStartCountdown, (LPVOID)&Detour_StartCountdown, (LPVOID*)&g_originalStartCountdown);
    if (status != MH_OK) {
        Log("[!] MH_CreateHook StartCountdown failed: %s", MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(g_targetStartCountdown);
    if (status != MH_OK) {
        Log("[!] MH_EnableHook StartCountdown failed: %s", MH_StatusToString(status));
        return false;
    }

    Log("[+] Targeted StartCountdown hook successfully active at +0x5137EC!");

    // Hook MCC Engine Dispatch at +0x42D6F8 (Runs on main simulation/render thread)
    g_targetMccEngineDispatch = (void*)(mccBase + Halo3Offsets::MCC_ENGINE_DISPATCH);
    status = MH_CreateHook(g_targetMccEngineDispatch, (LPVOID)&Detour_MccEngineDispatch, (LPVOID*)&g_originalMccEngineDispatch);
    if (status == MH_OK) {
        status = MH_EnableHook(g_targetMccEngineDispatch);
        if (status == MH_OK) {
            Log("[+] MCC Engine Dispatch hook successfully active at +0x42D6F8!");
        } else {
            Log("[!] MH_EnableHook MccEngineDispatch failed: %s", MH_StatusToString(status));
        }
    } else {
        Log("[!] MH_CreateHook MccEngineDispatch failed: %s", MH_StatusToString(status));
    }

    // Hook MCC RestartGameEngine at +0x1E6424 (Blocks advancing playlist after mission complete)
    g_targetRestartGameEngine = (void*)(mccBase + Halo3Offsets::MCC_RESTART_GAME_ENGINE);
    status = MH_CreateHook(g_targetRestartGameEngine, (LPVOID)&Detour_RestartGameEngine, (LPVOID*)&g_originalRestartGameEngine);
    if (status == MH_OK) {
        status = MH_EnableHook(g_targetRestartGameEngine);
        if (status == MH_OK) {
            Log("[+] MCC RestartGameEngine hook successfully active at +0x1E6424!");
        } else {
            Log("[!] MH_EnableHook RestartGameEngine failed: %s", MH_StatusToString(status));
        }
    } else {
        Log("[!] MH_CreateHook RestartGameEngine failed: %s", MH_StatusToString(status));
    }

    // Start background monitor thread to ensure halo3.dll remains hooked across maps/reloads
    CreateThread(nullptr, 0, Halo3MonitorThread, nullptr, 0, nullptr);

    return true;
}

void RemoveHooks() {
    g_stopMonitor = true;
    if (g_targetStartCountdown) {
        MH_DisableHook(g_targetStartCountdown);
        MH_RemoveHook(g_targetStartCountdown);
        g_targetStartCountdown = nullptr;
    }
    if (g_targetMccEngineDispatch) {
        MH_DisableHook(g_targetMccEngineDispatch);
        MH_RemoveHook(g_targetMccEngineDispatch);
        g_targetMccEngineDispatch = nullptr;
    }
    if (g_targetRestartGameEngine) {
        MH_DisableHook(g_targetRestartGameEngine);
        MH_RemoveHook(g_targetRestartGameEngine);
        g_targetRestartGameEngine = nullptr;
    }
    RemoveHalo3Hooks();
    MH_Uninitialize();
    Log("[Halo3AP Mod] All hooks cleanly removed.");
}

} // namespace Halo3AP
