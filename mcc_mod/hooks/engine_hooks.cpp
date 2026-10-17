#include "engine_hooks.h"
#include "event_hooks.h"
#include "shell_command.h"
#include "../engine/tag_system.h"
#include "../engine/object_table.h"
#include "../memory_utils.h"
#include "../offsets.h"
#include "../pipe_server.h"

namespace Halo3AP {

// These are set externally by hook_manager.
extern uintptr_t g_halo3Base;
extern bool      g_mapScanned;

Halo3EngineStep_t   g_originalHalo3EngineStep   = nullptr;
MccEngineDispatch_t g_originalMccEngineDispatch = nullptr;
UnitGetSeatCamera_t g_originalUnitGetSeatCamera = nullptr;
UnitUpdateSound_t   g_originalUnitUpdateSound   = nullptr;
void*               g_targetHalo3EngineStep     = nullptr;
void*               g_targetMccEngineDispatch   = nullptr;
void*               g_targetUnitGetSeatCamera   = nullptr;
void*               g_targetUnitUpdateSound     = nullptr;

void __fastcall Detour_Halo3EngineStep() {
    static bool s_loggedEngineStep = false;
    if (!s_loggedEngineStep) {
        s_loggedEngineStep = true;
        Log("[Halo3EngineStep] Running on game simulation thread. tid=%lu", GetCurrentThreadId());
    }

    // Ensure ShellCommand hook is active (engine object is guaranteed instantiated once simulation steps)
    if (!IsShellCommandHookActive() && g_halo3Base) {
        InstallShellCommandHook((HMODULE)g_halo3Base);
    }

    // On first simulation tick after map load, scan the tag table to discover all weapon tag indices.
    // Retries each tick until the tag table is populated (it may not be ready immediately after halo3.dll loads).
    if (!g_mapScanned) {
        if (ScanAndRegisterMapWeapons(g_halo3Base)) {
            g_mapScanned = true;
        }
    }

    if (g_originalHalo3EngineStep) {
        g_originalHalo3EngineStep();
    }

    // Run inventory enforcement directly from the game's simulation thread!
    CheckAndEnforcePlayerInventory(g_halo3Base);

    // Run shield enforcement directly from the game's simulation thread!
    EnforcePlayerShields(g_halo3Base);

    // Poll campaign terminal bitfield directly from the game's simulation thread!
    CheckPendingTerminals(g_halo3Base);
}

void __fastcall Detour_UnitUpdateSound(uint32_t unitDatum, uint32_t soundFlags) {
    // When updating shield sound (flag 0x80) for the local player:
    // If Chief's shields have recharged to his unlocked cap (< 100%), clamp to the cap
    // and clear bit 10 of flags (0x110) before the sound emitter processes it,
    // naturally silencing the recharge sound loop without artificially spoofing the shield value.
    if (g_halo3Base && (soundFlags & 0x80)) {
        uint32_t chiefDatum = g_localPlayerDatum.load();
        if (chiefDatum != 0xFFFFFFFF && chiefDatum != 0 && unitDatum == chiefDatum) {
            void* chiefObj = GetObjectPtr(g_halo3Base, chiefDatum);
            if (chiefObj && IsValidRead(chiefObj, Halo3Offsets::H3_UNIT_SHIELD + sizeof(float))) {
                float* pShield = (float*)((uintptr_t)chiefObj + Halo3Offsets::H3_UNIT_SHIELD);
                float maxFrac = g_maxShieldFraction.load();
                if (maxFrac < 1.0f) {
                    if (*pShield >= maxFrac - 0.001f) {
                        *pShield = maxFrac;
                        uint32_t* pFlags = (uint32_t*)((uintptr_t)chiefObj + Halo3Offsets::H3_UNIT_FLAGS);
                        if (IsValidRead(pFlags, sizeof(uint32_t))) {
                            *pFlags &= ~0x400; // Clear bit 10 (shield recharge sound flag)
                        }
                    }
                }
            }
        }
    }

    if (g_originalUnitUpdateSound) {
        g_originalUnitUpdateSound(unitDatum, soundFlags);
    }
}

void* __fastcall Detour_MccEngineDispatch(void* a1, void* a2, void* a3, void* a4) {
    static bool s_loggedMccDispatch = false;
    if (!s_loggedMccDispatch) {
        s_loggedMccDispatch = true;
        Log("[MccEngineDispatch] Running on main game thread.");
    }

    if (g_originalMccEngineDispatch) {
        return g_originalMccEngineDispatch(a1, a2, a3, a4);
    }
    return nullptr;
}

bool __fastcall Detour_UnitGetSeatCamera(uint32_t unitDatum, void* outTransform) {
    if (unitDatum == 0xFFFFFFFF || !g_halo3Base) {
        return false;
    }
    void* unitObj = GetObjectPtr(g_halo3Base, unitDatum);
    if (!unitObj) {
        return false;
    }
    // Check if unit has a valid parent / vehicle object
    uint32_t parentDatum = *(uint32_t*)((uintptr_t)unitObj + 0x10);
    if (parentDatum == 0xFFFFFFFF || parentDatum == 0) {
        return false;
    }
    void* parentObj = GetObjectPtr(g_halo3Base, parentDatum);
    if (!parentObj) {
        return false;
    }

    // Verify parent object's tag definition is loaded to prevent secondary nullptr dereference at 0x355ED4
    uint16_t tagIndex = *(uint16_t*)parentObj;
    uintptr_t* pTagTable = (uintptr_t*)(g_halo3Base + Halo3Offsets::H3_TAG_INSTANCES);
    if (pTagTable && IsValidRead(pTagTable, sizeof(uintptr_t))) {
        uintptr_t tagTable = *pTagTable;
        if (tagTable && IsValidRead((void*)(tagTable + (size_t)tagIndex * 8 + 4), sizeof(uint32_t))) {
            uint32_t tagDataOffset = *(uint32_t*)(tagTable + (size_t)tagIndex * 8 + 4);
            if (tagDataOffset == 0) {
                return false;
            }
        }
    }

    if (g_originalUnitGetSeatCamera) {
        return g_originalUnitGetSeatCamera(unitDatum, outTransform);
    }
    return false;
}

} // namespace Halo3AP
