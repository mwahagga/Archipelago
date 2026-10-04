#include "lobby_hooks.h"
#include "event_hooks.h"
#include "../offsets.h"
#include "../pipe_server.h"
#include "../memory_utils.h"
#include <tlhelp32.h>

namespace Halo3AP {

StartCountdown_t g_originalStartCountdown = nullptr;
void*            g_targetStartCountdown   = nullptr;

RestartGameEngine_t g_originalRestartGameEngine = nullptr;
void*               g_targetRestartGameEngine   = nullptr;

// Helper to extract selected chapter from CampaignLobby
std::string ExtractChapterFromLobby(void* lobby) {
    if (!lobby || IsBadReadPtr(lobby, 0x400)) return "";

    // 1. Check pointer fields in lobby (offsets 0x100 to 0x350)
    for (uintptr_t off = 0x100; off < 0x350; off += sizeof(uintptr_t)) {
        uintptr_t* ptrField = (uintptr_t*)((uintptr_t)lobby + off);
        const char* str = (const char*)(*ptrField);
        if (str && !IsBadReadPtr(str, 20)) {
            if (str[0] == '$' && strncmp(str, "$H3_Chapter_", 12) == 0) {
                return std::string(str);
            }
        }
    }

    // 2. Check inline strings embedded directly in lobby struct
    const char* raw = (const char*)lobby;
    for (size_t i = 0x100; i + 20 < 0x400; ++i) {
        if (raw[i] == '$' && strncmp(&raw[i], "$H3_Chapter_", 12) == 0) {
            return std::string(&raw[i]);
        }
    }
    return "";
}

// Maps menu chapter tokens to scenario IDs recognized by Archipelago
// Note: Chapter 3-1 is Arrival (Intro), Chapter 3-2 is Sierra 117, Chapter 3-3 is Crow's Nest, etc.
std::string MapChapterToScenario(const std::string& token) {
    if (token.find("3-10_") != std::string::npos) return "120_halo";     // Halo
    if (token.find("3-1_")  != std::string::npos) return "010_jungle";   // Arrival / Sierra 117 (Starter)
    if (token.find("3-2_")  != std::string::npos) return "010_jungle";   // Sierra 117
    if (token.find("3-3_")  != std::string::npos) return "020_base";     // Crow's Nest
    if (token.find("3-4_")  != std::string::npos) return "030_outskirts";// Tsavo Highway
    if (token.find("3-5_")  != std::string::npos) return "040_voi";      // The Storm
    if (token.find("3-6_")  != std::string::npos) return "050_floodvoi"; // Floodgate
    if (token.find("3-7_")  != std::string::npos) return "070_waste";    // The Ark
    if (token.find("3-8_")  != std::string::npos) return "100_citadel";  // The Covenant
    if (token.find("3-9_")  != std::string::npos) return "110_hc";       // Cortana
    return "";
}

// Detour for CampaignLobby::StartCountdown (+0x5137EC)
void __fastcall Detour_StartCountdown(void* lobby, uint64_t mode) {
    std::string chapter = ExtractChapterFromLobby(lobby);
    std::string scenario = MapChapterToScenario(chapter);

    Log("[Halo3AP Mod] StartCountdown requested for chapter='%s' (%s), mode=%llu",
        chapter.c_str(), scenario.c_str(), mode);

    if (!scenario.empty() && !IsMissionUnlocked(scenario)) {
        Log("============================================================");
        Log("[AP BOUNCER] ACCESS DENIED! Mission '%s' (%s) is LOCKED in Archipelago!",
            chapter.c_str(), scenario.c_str());
        Log("[AP BOUNCER] Cleanly blocking StartCountdown! Countdown will NOT start.");
        Log("============================================================");
        return; // Return without starting countdown -> stays smoothly in menu!
    }

    Log("[Halo3AP Mod] Access Granted for '%s'. Starting match countdown!", scenario.c_str());
    g_originalStartCountdown(lobby, mode);
}

// Detour for MCC RestartGameEngine (+0x1E6424)
void __fastcall Detour_RestartGameEngine(void* pThis, int32_t reason, void* r8, void* r9) {
    Log("[Halo3AP Mod] RestartGameEngine called! pThis=0x%p, reason=%d, gameWonFired=%d",
        pThis, reason, WasGameWonFired() ? 1 : 0);

    uintptr_t mccBase = (uintptr_t)GetModuleHandleA(nullptr);

    if (WasGameWonFired()) {
        Log("============================================================");
        Log("[Halo3AP Mod] Mission completed! Letting MCC handle restart naturally.");
        Log("============================================================");

        if (mccBase) {
            // Clear the queued scenario string so MCC doesn't try to reload the completed mission
            // or advance to the next one (the engine already failed to load_next via our 0x12EBC4 patch).
            uintptr_t* pStruct = (uintptr_t*)(mccBase + Halo3Offsets::MCC_SCENARIO_STRUCT);
            if (IsValidRead(pStruct, sizeof(uintptr_t)) && *pStruct) {
                char* scenStr = (char*)(*pStruct + Halo3Offsets::MCC_SCENARIO_STRING);
                if (IsValidRead(scenStr, 64) && *scenStr) {
                    Log("[Halo3AP Mod] Clearing scenario string: '%s'", scenStr);
                    memset(scenStr, 0, 64);
                }
            }
        }

        // Don't change reason — let MCC process RestartGameEngine with its original parameters.
        // The engine's fallback code at +0x12E619 (triggered when main_game_load_next_level
        // returned false) already put halo3.dll into a clean "no next level" state.
        ResetEventState(); // Cleanly reset event state for subsequent sessions
    } else if (mccBase) {
        // Secondary check: verify if the queued mission is unlocked in Archipelago
        uintptr_t* pStruct = (uintptr_t*)(mccBase + Halo3Offsets::MCC_SCENARIO_STRUCT);
        if (IsValidRead(pStruct, sizeof(uintptr_t)) && *pStruct) {
            const char* scenStr = (const char*)(*pStruct + Halo3Offsets::MCC_SCENARIO_STRING);
            if (scenStr && *scenStr && !IsMissionUnlocked(scenStr)) {
                Log("============================================================");
                Log("[AP BOUNCER] Queued mission '%s' is LOCKED in Archipelago! Clearing scenario string.", scenStr);
                Log("============================================================");
                memset((void*)scenStr, 0, strlen(scenStr));
            }
        }
    }

    if (g_originalRestartGameEngine) {
        g_originalRestartGameEngine(pThis, reason, r8, r9);
    }
}

} // namespace Halo3AP
