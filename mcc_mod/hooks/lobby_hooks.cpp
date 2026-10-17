#include "lobby_hooks.h"
#include "event_hooks.h"
#include "../offsets.h"
#include "../pipe_server.h"
#include "../memory_utils.h"
#include <tlhelp32.h>
#include <thread>

namespace Halo3AP {

static void ShowDisconnectedWarning() {
    MessageBeep(MB_ICONWARNING);
}

StartCountdown_t g_originalStartCountdown = nullptr;
void*            g_targetStartCountdown   = nullptr;

CommitSelection_t g_originalCommitSelection = nullptr;
void*            g_targetCommitSelection   = nullptr;

InitializeLevel_t g_originalInitializeLevel = nullptr;
void*             g_targetInitializeLevel   = nullptr;

RestartGameEngine_t g_originalRestartGameEngine = nullptr;
void*               g_targetRestartGameEngine   = nullptr;

extern bool         g_mapScanned;

// Helper to extract selected chapter from CampaignLobby
std::string ExtractChapterFromLobby(void* lobby) {
    if (!lobby || !IsMemoryReadable(lobby, 0x400)) return "";

    // 1. Check pointer fields in lobby (offsets 0x100 to 0x350)
    for (uintptr_t off = 0x100; off < 0x350; off += sizeof(uintptr_t)) {
        uintptr_t* ptrField = (uintptr_t*)((uintptr_t)lobby + off);
        if (!IsMemoryReadable(ptrField, sizeof(uintptr_t))) continue;
        const char* str = (const char*)(*ptrField);
        if (str && IsMemoryReadable(str, 20)) {
            if (str[0] == '$' && strstr(str, "_Chapter_") != nullptr) {
                return std::string(str);
            }
        }
    }

    // 2. Check inline strings embedded directly in lobby struct
    const char* raw = (const char*)lobby;
    for (size_t i = 0x100; i + 20 < 0x400; ++i) {
        if (raw[i] == '$' && strstr(&raw[i], "_Chapter_") != nullptr) {
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

// Maps numeric mission index (0..10) to scenario ID string
std::string MapMissionIndexToScenario(uint16_t index) {
    switch (index) {
        case 0:  return "010_jungle";    // Arrival (Prologue cutscene)
        case 1:  return "010_jungle";    // Sierra 117
        case 2:  return "020_base";      // Crow's Nest
        case 3:  return "030_outskirts"; // Tsavo Highway
        case 4:  return "040_voi";       // The Storm
        case 5:  return "050_floodvoi";  // Floodgate
        case 6:  return "070_waste";     // The Ark
        case 7:  return "100_citadel";   // The Covenant
        case 8:  return "110_hc";        // Cortana
        case 9:  return "120_halo";      // Halo
        case 10: return "120_halo";      // Epilogue
        default: return "";
    }
}

// Returns the index (1..9) of the first available unlocked mission in Archipelago
uint16_t GetFirstUnlockedMissionIndex() {
    for (uint16_t i = 1; i <= 9; ++i) {
        std::string scen = MapMissionIndexToScenario(i);
        if (!scen.empty() && IsMissionUnlocked(scen)) {
            return i;
        }
    }
    return 1; // Default Sierra 117
}

// Helper to query the clicked mission item index from CampaignLobby's Scaleform list controller
uint16_t GetClickedMissionIndex(void* lobby) {
    if (!lobby || !IsValidRead(lobby, 0x2480)) return 0xFFFF;

    uintptr_t mccBase = (uintptr_t)GetModuleHandleA(nullptr);
    if (!mccBase) return 0xFFFF;

    // 1. Get List Controller: call *0xF0(*(lobby + 0x910))(lobby + 0x910, 0, 0)
    void* pSubObj = (void*)((uintptr_t)lobby + 0x910);
    if (!IsValidRead(pSubObj, sizeof(uintptr_t))) return 0xFFFF;

    uintptr_t vtable = *(uintptr_t*)pSubObj;
    if (!vtable || !IsValidRead((void*)vtable, 0xF8)) return 0xFFFF;

    typedef void* (__fastcall *GetListCtrl_t)(void* pThis, int arg1, int arg2);
    GetListCtrl_t fnGetListCtrl = *(GetListCtrl_t*)(vtable + 0xF0);
    if (!fnGetListCtrl || !IsValidRead((void*)fnGetListCtrl, 8)) return 0xFFFF;

    void* listCtrl = fnGetListCtrl(pSubObj, 0, 0);
    if (!listCtrl || !IsValidRead(listCtrl, sizeof(uintptr_t))) return 0xFFFF;

    // 2. Get Selected Item: call mccBase + MCC_GET_SELECTED_ITEM (0x4BC600)(listCtrl)
    typedef void* (__fastcall *GetSelectedItem_t)(void* listCtrl);
    GetSelectedItem_t fnGetSelectedItem = (GetSelectedItem_t)(mccBase + Halo3Offsets::MCC_GET_SELECTED_ITEM);
    void* item = fnGetSelectedItem(listCtrl);
    if (!item || !IsValidRead(item, sizeof(uintptr_t))) return 0xFFFF;

    // 3. Get Item Index: call *0x160(*(item))(item) -> returns index in AX
    uintptr_t itemVtable = *(uintptr_t*)item;
    if (!itemVtable || !IsValidRead((void*)itemVtable, 0x168)) return 0xFFFF;

    typedef uint16_t (__fastcall *GetIndex_t)(void* pItem);
    GetIndex_t fnGetIndex = *(GetIndex_t*)(itemVtable + 0x160);
    if (!fnGetIndex || !IsValidRead((void*)fnGetIndex, 8)) return 0xFFFF;

    return fnGetIndex(item);
}

enum class MCCGame {
    Unknown,
    Halo1,
    Halo2,
    Halo3,
    ODST,
    Halo4,
    Reach
};

static const char* GetGameName(MCCGame g) {
    switch (g) {
        case MCCGame::Halo1: return "Halo: Combat Evolved";
        case MCCGame::Halo2: return "Halo 2";
        case MCCGame::Halo3: return "Halo 3";
        case MCCGame::ODST:  return "Halo 3: ODST";
        case MCCGame::Halo4: return "Halo 4";
        case MCCGame::Reach: return "Halo: Reach";
        default:             return "Unknown Campaign";
    }
}

static MCCGame DetectLobbyGame(void* lobby) {
    if (!lobby || !IsValidRead(lobby, 0x300)) return MCCGame::Unknown;

    // 1. Check header string at +0x1E8
    const char** pHeader = (const char**)((uintptr_t)lobby + 0x1E8);
    if (IsMemoryReadable(pHeader, sizeof(uintptr_t)) && *pHeader && IsMemoryReadable(*pHeader, 8)) {
        const char* h = *pHeader;
        if (strstr(h, "ODST") || strstr(h, "odst")) return MCCGame::ODST;
        if (strstr(h, "H3") || strstr(h, "HALO3") || strstr(h, "Halo 3")) return MCCGame::Halo3;
        if (strstr(h, "REACH") || strstr(h, "Reach") || strstr(h, "reach")) return MCCGame::Reach;
        if (strstr(h, "H4") || strstr(h, "HALO4") || strstr(h, "Halo 4")) return MCCGame::Halo4;
        if (strstr(h, "H2") || strstr(h, "HALO2") || strstr(h, "Halo 2")) return MCCGame::Halo2;
        if (strstr(h, "H1") || strstr(h, "HALO1") || strstr(h, "Halo 1") || strstr(h, "Halo CE")) return MCCGame::Halo1;
    }

    // 2. Check chapter token from lobby
    std::string chapter = ExtractChapterFromLobby(lobby);
    if (!chapter.empty()) {
        if (chapter.find("ODST") != std::string::npos || chapter.find("odst") != std::string::npos) return MCCGame::ODST;
        if (chapter.find("$H3_") != std::string::npos || chapter.find("Halo3") != std::string::npos) return MCCGame::Halo3;
        if (chapter.find("Reach") != std::string::npos || chapter.find("reach") != std::string::npos || chapter.find("$HR_") != std::string::npos) return MCCGame::Reach;
        if (chapter.find("$H4_") != std::string::npos || chapter.find("Halo4") != std::string::npos) return MCCGame::Halo4;
        if (chapter.find("$H2_") != std::string::npos || chapter.find("Halo2") != std::string::npos) return MCCGame::Halo2;
        if (chapter.find("$H1_") != std::string::npos || chapter.find("Halo1") != std::string::npos) return MCCGame::Halo1;
    }

    return MCCGame::Unknown;
}

typedef void (__fastcall *UpdateSelectedMission_t)(void* lobby);

static void RefreshLobbyUI(void* lobby) {
    uintptr_t mccBase = (uintptr_t)GetModuleHandleA(nullptr);
    if (mccBase && lobby) {
        UpdateSelectedMission_t fnUpdate = (UpdateSelectedMission_t)(mccBase + Halo3Offsets::MCC_UPDATE_SELECTED_MISSION);
        if (fnUpdate && IsValidRead((void*)fnUpdate, 8)) {
            fnUpdate(lobby);
        }
    }
}

// Detour for CampaignLobby::CommitSelection (+0x50E1E4)
void __fastcall Detour_CommitSelection(void* lobby) {
    if (!lobby || !IsValidRead(lobby, 0x2480)) {
        if (g_originalCommitSelection) g_originalCommitSelection(lobby);
        return;
    }

    uint32_t screenState = *(uint32_t*)((uintptr_t)lobby + 0x2464);
    if (screenState == Halo3Offsets::MCC_SCREEN_MISSION_SELECT) {
        MCCGame game = DetectLobbyGame(lobby);
        if (game != MCCGame::Halo3) {
            // For other campaigns, allow normal selection through without touching or refreshing Halo 3 UI
            if (g_originalCommitSelection) {
                g_originalCommitSelection(lobby);
            }
            return;
        }

        if (!IsClientConnected()) {
            Log("[AP BOUNCER] Mission selection blocked: Client is not connected to Archipelago!");
            ShowDisconnectedWarning();
            RefreshLobbyUI(lobby);
            return;
        }

        uint16_t clickedIndex = GetClickedMissionIndex(lobby);
        if (clickedIndex != 0xFFFF) {
            std::string scenario = MapMissionIndexToScenario(clickedIndex);
            Log("[Halo3AP Mod] Mission selection clicked: index=%u (%s)", clickedIndex, scenario.c_str());

            if (!scenario.empty() && !IsMissionUnlocked(scenario)) {
                Log("============================================================");
                Log("[AP BOUNCER] MISSION SELECTION BLOCKED! Mission '%s' (index %u) is LOCKED in Archipelago!",
                    scenario.c_str(), clickedIndex);
                Log("[AP BOUNCER] Refusing to commit selection to lobby. Staying on unlocked mission.");
                Log("============================================================");
                ShowDisconnectedWarning();
                RefreshLobbyUI(lobby);
                return; // Return without committing to lobby -> stays on currently unlocked mission!
            }
        }
    }

    if (g_originalCommitSelection) {
        g_originalCommitSelection(lobby);
    }
}

// Detour for CampaignLobby::InitializeLevel (+0x509C24)
// Automatically overrides preselected mission if the profile-saved mission is locked in Archipelago.
void __fastcall Detour_InitializeLevel(void* lobby, void* rdx) {
    if (g_originalInitializeLevel) {
        g_originalInitializeLevel(lobby, rdx);
    }

    if (!lobby || !IsValidRead(lobby, 0x2480)) return;

    if (!IsClientConnected()) {
        Log("[AP BOUNCER] Lobby entered while disconnected from Archipelago.");
        ShowDisconnectedWarning();
        return;
    }

    MCCGame game = DetectLobbyGame(lobby);
    if (game != MCCGame::Halo3) {
        Log("[AP BOUNCER] Lobby entered for %s (not active in current AP session).", GetGameName(game));
        ShowDisconnectedWarning();
        return;
    }

    uint16_t missionIndex = *(uint16_t*)((uintptr_t)lobby + 0x247C);
    std::string scenario = MapMissionIndexToScenario(missionIndex);

    if (!scenario.empty() && !IsMissionUnlocked(scenario)) {
        uint16_t unlockedIndex = GetFirstUnlockedMissionIndex();
        std::string unlockedScen = MapMissionIndexToScenario(unlockedIndex);

        Log("============================================================");
        Log("[AP BOUNCER] Preselected mission '%s' (index %u) is LOCKED!",
            scenario.c_str(), missionIndex);
        Log("[AP BOUNCER] Auto-redirecting preselection to '%s' (index %u)!",
            unlockedScen.c_str(), unlockedIndex);
        Log("============================================================");

        *(uint16_t*)((uintptr_t)lobby + 0x247C) = unlockedIndex;
        RefreshLobbyUI(lobby);
    } else {
        Log("[Halo3AP Mod] Preselected mission '%s' (index %u) is UNLOCKED.",
            scenario.c_str(), missionIndex);
    }
}

// Detour for CampaignLobby::StartCountdown (+0x5137EC)
void __fastcall Detour_StartCountdown(void* lobby, uint64_t mode) {
    if (!IsClientConnected()) {
        Log("============================================================");
        Log("[AP BOUNCER] START GAME BLOCKED! Client is not connected to Archipelago!");
        Log("============================================================");
        ShowDisconnectedWarning();
        return; // Return without starting countdown -> stays smoothly in menu!
    }

    MCCGame game = DetectLobbyGame(lobby);
    std::string chapter = ExtractChapterFromLobby(lobby);
    std::string scenario = MapChapterToScenario(chapter);

    // If game detection was Unknown, but chapter token mapped directly to a recognized Halo 3 scenario, treat as Halo 3!
    if (game == MCCGame::Unknown && !scenario.empty()) {
        game = MCCGame::Halo3;
    }

    if (game != MCCGame::Halo3) {
        Log("============================================================");
        Log("[AP BOUNCER] START GAME BLOCKED! %s is not active in this Archipelago session!",
            GetGameName(game));
        Log("============================================================");
        ShowDisconnectedWarning();
        return;
    }

    Log("[Halo3AP Mod] StartCountdown requested for chapter='%s' (%s), mode=%llu",
        chapter.c_str(), scenario.c_str(), mode);

    if (!scenario.empty() && !IsMissionUnlocked(scenario)) {
        Log("============================================================");
        Log("[AP BOUNCER] ACCESS DENIED! Mission '%s' (%s) is LOCKED in Archipelago!",
            chapter.c_str(), scenario.c_str());
        Log("[AP BOUNCER] Cleanly blocking StartCountdown! Countdown will NOT start.");
        Log("============================================================");
        ShowDisconnectedWarning();
        return; // Return without starting countdown -> stays smoothly in menu!
    }

    Log("[Halo3AP Mod] Access Granted for '%s'. Starting match countdown!", scenario.c_str());
    ResetEventState();
    g_mapScanned = false;
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
        g_mapScanned = false;
    } else {
        ResetEventState();
        g_mapScanned = false;
        if (mccBase) {
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
    }

    if (g_originalRestartGameEngine) {
        g_originalRestartGameEngine(pThis, reason, r8, r9);
    }
}

} // namespace Halo3AP
