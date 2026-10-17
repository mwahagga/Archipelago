#include "event_hooks.h"
#include "../pipe_server.h"
#include "../memory_utils.h"
#include "../offsets.h"
#include "../engine/object_table.h"
#include "shell_command.h"
#include <set>
#include <unordered_set>

namespace Halo3AP {

// ============================================================================
// EVENT DETECTION — Rally Points, Terminals, Skulls
//
// These detect in-game events that correspond to Archipelago location checks.
// Detected events are stored in g_modState.pendingEvents and reported to
// Halo3Client.py via the Named Pipe IPC protocol.
// ============================================================================

GameInsertionPointUnlock_t g_originalInsertionPointUnlock = nullptr;
void*                      g_targetInsertionPointUnlock   = nullptr;

extern uintptr_t g_halo3Base;

TerminalActivate_t   g_originalTerminalActivate   = nullptr;
void*                g_targetTerminalActivate     = nullptr;

AwardPrimarySkull_t     g_originalAwardPrimarySkull  = nullptr;
void*                   g_targetAwardPrimarySkull    = nullptr;

AwardSecondarySkull_t   g_originalAwardSecondarySkull = nullptr;
void*                   g_targetAwardSecondarySkull   = nullptr;

// ============================================================================
// RALLY POINT DETECTION
// ============================================================================

static std::set<int32_t> s_unlockedRallies;
static std::set<int32_t> s_accessedTerminals;
static bool s_gameWonFired = false;
static uint16_t s_initialTerminalBitmask = 0;
static bool s_terminalBitmaskInitialized = false;

static std::string GetCurrentScenarioCode() {
    uintptr_t mccBase = (uintptr_t)GetModuleHandleA(nullptr);
    if (!mccBase) return "";
    uintptr_t* pStruct = (uintptr_t*)(mccBase + Halo3Offsets::MCC_SCENARIO_STRUCT);
    if (!IsValidRead(pStruct, sizeof(uintptr_t)) || !*pStruct) return "";
    uintptr_t structAddr = *pStruct;
    if (!IsValidRead((void*)(structAddr + Halo3Offsets::MCC_SCENARIO_STRING), 64)) return "";
    const char* str = (const char*)(structAddr + Halo3Offsets::MCC_SCENARIO_STRING);
    std::string s = str ? str : "";
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    return s;
}

bool WasGameWonFired() {
    return s_gameWonFired;
}

static std::mutex s_skullSessionMtx;
static std::unordered_set<std::string> s_awardedSkullsInSession;

void ResetEventState() {
    std::lock_guard<std::mutex> lock(g_modState.mtx);
    s_unlockedRallies.clear();
    s_accessedTerminals.clear();
    s_gameWonFired = false;
    s_terminalBitmaskInitialized = false;
    s_initialTerminalBitmask = 0;
    g_quitAfterComplete.store(false);
    {
        std::lock_guard<std::mutex> sLock(s_skullSessionMtx);
        s_awardedSkullsInSession.clear();
    }
    ResetShieldAndDeathLinkState();
    Log("[EVENT] Event state reset for new level session.");
}

// game_insertion_point_unlock (halo3.dll + 0xF0810) is called by the engine when the player
// crosses into a new rally point zone (e.g. Bravo=1, Charlie=2).
// Parameter: rallyIndex arrives in ecx (1st parameter).
void __fastcall Detour_GameInsertionPointUnlock(int32_t rallyIndex) {
    Log("[EVENT] game_insertion_point_unlock called: index=%d", rallyIndex);

    if (rallyIndex == 1 || rallyIndex == 2) {
        bool isNew = false;
        {
            std::lock_guard<std::mutex> lock(g_modState.mtx);
            if (s_unlockedRallies.find(rallyIndex) == s_unlockedRallies.end()) {
                s_unlockedRallies.insert(rallyIndex);
                isNew = true;
                std::string scen = GetCurrentScenarioCode();
                std::string eventKey = "rally_" + std::to_string(rallyIndex);
                if (!scen.empty()) {
                    eventKey += ":" + scen;
                }
                g_modState.pendingEvents.insert(eventKey);
            }
        }
        if (isNew) {
            Log("[EVENT] Rally Point %s reached! (index=%d)",
                rallyIndex == 1 ? "Bravo" : (rallyIndex == 2 ? "Charlie" : "Unknown"), rallyIndex);
        }
    }

    if (g_originalInsertionPointUnlock) {
        g_originalInsertionPointUnlock(rallyIndex);
    }
}

// ============================================================================
// TERMINAL DETECTION
// ============================================================================

// terminal_activate (halo3.dll + 0x3A8798): native engine function called when
// the player interacts with a terminal device (e.g. holds E / RB).
// Parameter: deviceDatum arrives in ecx.
void __fastcall Detour_TerminalActivate(uint32_t deviceDatum) {
    Log("[EVENT] Detour_TerminalActivate called! deviceDatum=0x%08X", deviceDatum);

    int terminalIndex = -1;
    uintptr_t halo3Base = g_halo3Base;

    // Follow engine tag lookup:
    // TLS -> threadCtx -> objTable (+0x38) -> objArray (+0x48) -> objMem (+datum*24+0x10) -> tagIndex
    // -> tagGlobals (0xA49018) -> tagOffset -> tagCacheBase (0x1FCF4C8) -> tagDef (+tagOffset*4)
    // -> sequence number at tagDef + 0x198.
    if (halo3Base) {
        uint32_t tlsIndex = *(uint32_t*)(halo3Base + Halo3Offsets::H3_TLS_INDEX);
        uintptr_t* tlsArray = (uintptr_t*)__readgsqword(0x58);
        if (tlsArray && IsValidRead(tlsArray, sizeof(uintptr_t) * (tlsIndex + 1))) {
            uintptr_t threadCtx = tlsArray[tlsIndex];
            if (threadCtx && IsValidRead((void*)threadCtx, 0x50)) {
                uintptr_t objTable = *(uintptr_t*)(threadCtx + 0x38);
                uint16_t datumIndex = (uint16_t)deviceDatum;
                if (objTable && IsValidRead((void*)objTable, 0x50)) {
                    uintptr_t objArray = *(uintptr_t*)(objTable + 0x48);
                    if (objArray && IsValidRead((void*)(objArray + datumIndex * 24), 24)) {
                        uintptr_t objMem = *(uintptr_t*)(objArray + datumIndex * 24 + 0x10);
                        if (objMem && IsValidRead((void*)objMem, 4)) {
                            uint16_t tagIndex = *(uint16_t*)objMem;
                            uintptr_t tagGlobals = *(uintptr_t*)(halo3Base + Halo3Offsets::H3_TAG_INSTANCES);
                            if (tagGlobals && IsValidRead((void*)(tagGlobals + tagIndex * 8), 8)) {
                                uint32_t tagOffset = *(uint32_t*)(tagGlobals + tagIndex * 8 + 4);
                                if (tagOffset) {
                                    uintptr_t tagInstances = *(uintptr_t*)(halo3Base + Halo3Offsets::H3_TAG_CACHE_BASE);
                                    if (tagInstances) {
                                        uintptr_t tagDef = tagInstances + (uint64_t)tagOffset * 4;
                                        if (IsValidRead((void*)(tagDef + 0x198), 2)) {
                                            uint16_t termNum = *(uint16_t*)(tagDef + 0x198);
                                            if (termNum > 0) {
                                                terminalIndex = (int)termNum - 1;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (g_originalTerminalActivate) {
        g_originalTerminalActivate(deviceDatum);
    }

    // If tag lookup didn't yield an index, check the campaign bitmask immediately after activate
    if (terminalIndex < 0 && halo3Base) {
        uint32_t tlsIndex = *(uint32_t*)(halo3Base + Halo3Offsets::H3_TLS_INDEX);
        uintptr_t* tlsArray = (uintptr_t*)__readgsqword(0x58);
        if (tlsArray && IsValidRead(tlsArray, sizeof(uintptr_t) * (tlsIndex + 1))) {
            uintptr_t threadCtx = tlsArray[tlsIndex];
            if (threadCtx && IsValidRead((void*)threadCtx, 0x50)) {
                uintptr_t campaignStruct = *(uintptr_t*)(threadCtx + 0x40);
                if (campaignStruct && IsValidRead((void*)(campaignStruct + Halo3Offsets::H3_CAMPAIGN_ACCESSED_TERMINALS), 2)) {
                    uint16_t accessed = *(uint16_t*)(campaignStruct + Halo3Offsets::H3_CAMPAIGN_ACCESSED_TERMINALS);
                    uint16_t diff = accessed & ~s_initialTerminalBitmask;
                    for (int i = 0; i < 16; ++i) {
                        if (diff & (1 << i)) {
                            terminalIndex = i;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (terminalIndex >= 0) {
        bool isNew = false;
        {
            std::lock_guard<std::mutex> lock(g_modState.mtx);
            if (s_accessedTerminals.find(terminalIndex) == s_accessedTerminals.end()) {
                s_accessedTerminals.insert(terminalIndex);
                isNew = true;
                std::string scen = GetCurrentScenarioCode();
                std::string eventKey = "terminal_" + std::to_string(terminalIndex);
                if (!scen.empty()) {
                    eventKey += ":" + scen;
                }
                g_modState.pendingEvents.insert(eventKey);
            }
        }
        if (isNew) {
            Log("[EVENT] Terminal %d accessed! (deviceDatum=0x%08X)", terminalIndex, deviceDatum);
        }
    }
}

// Polling fallback to check terminal bitfield every simulation tick
void CheckPendingTerminals(uintptr_t halo3Base) {
    if (!halo3Base) return;
    uint32_t tlsIndex = *(uint32_t*)(halo3Base + Halo3Offsets::H3_TLS_INDEX);
    uintptr_t* tlsArray = (uintptr_t*)__readgsqword(0x58);
    if (!tlsArray || !IsValidRead(tlsArray, sizeof(uintptr_t) * (tlsIndex + 1))) return;
    uintptr_t threadCtx = tlsArray[tlsIndex];
    if (!threadCtx || !IsValidRead((void*)threadCtx, 0x50)) return;
    uintptr_t campaignStruct = *(uintptr_t*)(threadCtx + 0x40);
    if (!campaignStruct || !IsValidRead((void*)(campaignStruct + Halo3Offsets::H3_CAMPAIGN_ACCESSED_TERMINALS), 2)) return;

    // Accessed terminals bitmask is at campaignStruct + 0x132
    uint16_t accessed = *(uint16_t*)(campaignStruct + Halo3Offsets::H3_CAMPAIGN_ACCESSED_TERMINALS);
    if (!s_terminalBitmaskInitialized) {
        s_initialTerminalBitmask = accessed;
        s_terminalBitmaskInitialized = true;
    }

    uint16_t newlyAccessed = accessed & ~s_initialTerminalBitmask;
    if (newlyAccessed == 0) return;

    for (int i = 0; i < 16; ++i) {
        if (newlyAccessed & (1 << i)) {
            bool isNew = false;
            {
                std::lock_guard<std::mutex> lock(g_modState.mtx);
                if (s_accessedTerminals.find(i) == s_accessedTerminals.end()) {
                    s_accessedTerminals.insert(i);
                    isNew = true;
                    std::string scen = GetCurrentScenarioCode();
                    std::string eventKey = "terminal_" + std::to_string(i);
                    if (!scen.empty()) {
                        eventKey += ":" + scen;
                    }
                    g_modState.pendingEvents.insert(eventKey);
                }
            }
            if (isNew) {
                Log("[EVENT] Terminal %d accessed (polled from campaign bitmask)! bitmask=0x%04X", i, accessed);
            }
        }
    }
}

// ============================================================================
// SKULL DETECTION
// ============================================================================

// campaign_metagame_award_primary_skull (halo3.dll + 0x136088): called when a primary skull is collected.
// Parameters: playerDatum in ecx, skullIndex in edx (0 = Iron, 1 = Black Eye, etc.)
void __fastcall Detour_AwardPrimarySkull(int32_t playerDatum, int32_t skullIndex) {
    Log("[EVENT] Primary skull collected! player=0x%08X, skullIndex=%d", playerDatum, skullIndex);

    {
        std::lock_guard<std::mutex> lock(g_modState.mtx);
        std::string scen = GetCurrentScenarioCode();
        std::string eventKey = "skull_primary_" + std::to_string(skullIndex);
        if (!scen.empty()) {
            eventKey += ":" + scen;
        }
        g_modState.pendingEvents.insert(eventKey);
    }

    if (g_originalAwardPrimarySkull) {
        g_originalAwardPrimarySkull(playerDatum, skullIndex);
    }
}

// campaign_metagame_award_secondary_skull (halo3.dll + 0x136580): called when a secondary skull is collected.
// Parameters: playerDatum in ecx, skullIndex in edx (0 = Blind, 1 = Grunt Birthday Party, etc.)
void __fastcall Detour_AwardSecondarySkull(int32_t playerDatum, int32_t skullIndex) {
    Log("[EVENT] Secondary skull collected! player=0x%08X, skullIndex=%d", playerDatum, skullIndex);

    {
        std::lock_guard<std::mutex> lock(g_modState.mtx);
        std::string scen = GetCurrentScenarioCode();
        std::string eventKey = "skull_secondary_" + std::to_string(skullIndex);
        if (!scen.empty()) {
            eventKey += ":" + scen;
        }
        g_modState.pendingEvents.insert(eventKey);
    }

    if (g_originalAwardSecondarySkull) {
        g_originalAwardSecondarySkull(playerDatum, skullIndex);
    }
}

// Direct pickup / inventory skull award verification
// Called whenever Chief equips a weapon or periodically during inventory checks.
// Eliminates dependence on Bungie's campaign HS scripts (which fail to wake on Easy or from checkpoints).
void CheckAndAwardSkullIfHolding(uintptr_t halo3Base, uint32_t playerDatum, void* weaponObj, void* tagDef) {
    if (!weaponObj || !tagDef) return;

    if (!IsValidRead(tagDef, 0x190)) return;
    uint16_t weaponType = *(uint16_t*)((uintptr_t)tagDef + 0x18C);
    // 11 = _weapon_type_ball (skulls and oddball)
    if (weaponType != 11) return;

    if (!IsValidRead(weaponObj, 0x100)) return;
    uint16_t nameIdx = *(uint16_t*)((uintptr_t)weaponObj + 0xE8);

    std::string scen = GetCurrentScenarioCode();
    if (scen.empty()) return;

    bool isSecondary = false;
    if (IsValidRead((void*)((uintptr_t)tagDef + 0x4E8), sizeof(uint32_t))) {
        uint32_t val4e8 = *(uint32_t*)((uintptr_t)tagDef + 0x4E8);
        if (val4e8 != 0) {
            isSecondary = true;
        }
        Log("[Skull] Inspecting skull tagDef=%p, val4e8=0x%08X -> isSecondary=%d",
            tagDef, val4e8, isSecondary ? 1 : 0);
    }

    int primaryIndex = -1;
    int secondaryIndex = -1;

    if (scen.find("010_jungle") != std::string::npos) {
        // Sierra 117: Iron (Primary 0) vs Blind (Secondary 0)
        if (isSecondary) {
            secondaryIndex = 0;
        } else {
            primaryIndex = 0;
        }
    } else if (scen.find("020_base") != std::string::npos) {
        // Crow's Nest: Black Eye (Primary 1) vs Grunt Birthday (Secondary 1)
        if (isSecondary) {
            secondaryIndex = 1;
        } else {
            primaryIndex = 1;
        }
    } else if (scen.find("030_outskirts") != std::string::npos || scen.find("030_docks") != std::string::npos) {
        // Tsavo Highway: Tough Luck (Primary 2)
        primaryIndex = 2;
    } else if (scen.find("040_voi") != std::string::npos) {
        // The Storm: Catch (Primary 3)
        primaryIndex = 3;
    } else if (scen.find("050_flood") != std::string::npos) {
        // Floodgate: Fog (Primary 4)
        primaryIndex = 4;
    } else if (scen.find("070_waste") != std::string::npos || scen.find("060_island") != std::string::npos) {
        // The Ark: Famine (Primary 5) vs Cowbell (Secondary 2)
        if (isSecondary) {
            secondaryIndex = 2;
        } else {
            primaryIndex = 5;
        }
    } else if (scen.find("100_citadel") != std::string::npos) {
        // The Covenant: Thunderstorm (Primary 6) vs IWHBYD (Secondary 3)
        if (isSecondary) {
            secondaryIndex = 3;
        } else {
            primaryIndex = 6;
        }
    } else if (scen.find("110_hc") != std::string::npos || scen.find("080_ship") != std::string::npos) {
        // Cortana: Tilt (Primary 7)
        primaryIndex = 7;
    } else if (scen.find("120_halo") != std::string::npos || scen.find("090_sacrifice") != std::string::npos) {
        // Halo: Mythic (Primary 8)
        primaryIndex = 8;
    }

    if (primaryIndex >= 0) {
        std::string key = "primary_" + std::to_string(primaryIndex) + ":" + scen;
        bool isFirst = false;
        {
            std::lock_guard<std::mutex> lock(s_skullSessionMtx);
            isFirst = s_awardedSkullsInSession.insert(key).second;
        }
        if (isFirst) {
            Log("[EVENT] Direct skull pickup detected! player=0x%08X, primaryIndex=%d (nameIdx=%u, scen=%s)",
                playerDatum, primaryIndex, nameIdx, scen.c_str());
            Detour_AwardPrimarySkull(playerDatum, primaryIndex);
        }
    } else if (secondaryIndex >= 0) {
        std::string key = "secondary_" + std::to_string(secondaryIndex) + ":" + scen;
        bool isFirst = false;
        {
            std::lock_guard<std::mutex> lock(s_skullSessionMtx);
            isFirst = s_awardedSkullsInSession.insert(key).second;
        }
        if (isFirst) {
            Log("[EVENT] Direct skull pickup detected! player=0x%08X, secondaryIndex=%d (nameIdx=%u, scen=%s)",
                playerDatum, secondaryIndex, nameIdx, scen.c_str());
            Detour_AwardSecondarySkull(playerDatum, secondaryIndex);
        }
    }
}

// ============================================================================
// MISSION COMPLETION — game_won hook
// ============================================================================

// These are set externally by hook_manager.
extern uintptr_t g_halo3Base;

GameWon_t g_originalGameWon = nullptr;
void*     g_targetGameWon   = nullptr;

// game_won (+0x1F2F7C): HS impl called when a campaign mission is beaten.
void __fastcall Detour_GameWon(void* rcxParam, int32_t edxParam) {
    Log("[EVENT] ========================================");
    Log("[EVENT] game_won called! Mission complete!");
    Log("[EVENT] ========================================");

    // Emit event for the Python client (only once per mission)
    if (!s_gameWonFired) {
        s_gameWonFired = true;
        {
            std::lock_guard<std::mutex> lock(g_modState.mtx);
            std::string scen = GetCurrentScenarioCode();
            std::string eventKey = "game_won";
            if (!scen.empty()) {
                eventKey += ":" + scen;
            }
            g_modState.pendingEvents.insert(eventKey);
        }
        // Signal IGameEngine::ShellCommand hook to cleanly teardown to menu
        g_quitAfterComplete.store(true);
    }

    // Let the original game_won execute so engine state is properly updated
    if (g_originalGameWon) {
        g_originalGameWon(rcxParam, edxParam);
    }
}

// ============================================================================
// CLEAN MENU RETURN — LoadNextCampaignLevel Hook
// ============================================================================

LoadNextCampaignLevel_t g_originalLoadNextCampaignLevel = nullptr;
void*                  g_targetLoadNextCampaignLevel   = nullptr;

// load_next_campaign_level (halo3.dll + 0x1398E4):
// Called by 0xF05CA when the engine is about to load the next campaign map.
// Intercepting this function prevents Halo 3 from preparing the next mission.
void __fastcall Detour_LoadNextCampaignLevel(void* rcx, void* rdx, void* r8) {
    Log("[EVENT] ========================================");
    Log("[EVENT] LoadNextCampaignLevel intercepted (+0x1398E4)!");
    Log("[EVENT] Blocking next mission loading.");
    Log("[EVENT] ========================================");
}

} // namespace Halo3AP

