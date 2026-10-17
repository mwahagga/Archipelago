#include "weapon_hooks.h"
#include "event_hooks.h"
#include "../engine/tag_system.h"
#include "../engine/object_table.h"
#include "../memory_utils.h"
#include "../pipe_server.h"
#include "../offsets.h"
#include <intrin.h>

namespace Halo3AP {

// These are set externally by hook_manager via the hook table.
// The extern variable g_halo3Base is accessed through a getter to avoid circular deps.
// We receive halo3Base through function parameters where possible, but detour
// callbacks don't get it as a parameter — they need a global.
// hook_manager.cpp owns the canonical g_halo3Base; we access it here.
extern uintptr_t g_halo3Base;

// Original function pointers (populated by MinHook)
WeaponEvaluateCandidate_t  g_originalWeaponEvaluateCandidate  = nullptr;
CanTradeWithAlly_t         g_originalCanTradeWithAlly         = nullptr;
RaycastEvaluateCandidate_t g_originalRaycastEvaluateCandidate = nullptr;
DeviceEvaluateCandidate_t  g_originalDeviceEvaluateCandidate  = nullptr;
UnitCanPickupWeapon_t      g_originalUnitCanPickupWeapon      = nullptr;
UnitUpdateWeaponPrompt_t   g_originalUnitUpdateWeaponPrompt   = nullptr;
UnitActionSwapWeapon_t     g_originalUnitActionSwapWeapon     = nullptr;
UnitActionPickupWeapon_t   g_originalUnitActionPickupWeapon   = nullptr;
UnitAddWeapon_t            g_originalUnitAddWeapon            = nullptr;

// Hook target pointers (for MH_CreateHook / MH_RemoveHook)
void* g_targetWeaponEvaluateCandidate  = nullptr;
void* g_targetCanTradeWithAlly         = nullptr;
void* g_targetRaycastEvaluateCandidate = nullptr;
void* g_targetDeviceEvaluateCandidate  = nullptr;
void* g_targetUnitCanPickupWeapon      = nullptr;
void* g_targetUnitUpdateWeaponPrompt   = nullptr;
void* g_targetUnitActionSwapWeapon     = nullptr;
void* g_targetUnitActionPickupWeapon   = nullptr;
void* g_targetUnitAddWeapon            = nullptr;

// ============================================================================
// DETOUR IMPLEMENTATIONS
// ============================================================================

// Detour for weapon_evaluate_candidate (+0xEA6E4):
// Master candidate evaluation function for ground weapons, called every frame during crosshair/proximity scanning.
// Returning false (0) completely rejects the weapon from interaction:
// 1. Candidate is NEVER registered in Chief's interaction state.
// 2. HUD pickup/swap prompt ("Hold [E] to swap / pick up") NEVER displays.
// 3. Button presses/holds ignore the weapon, allowing reload to proceed uninterrupted!
bool __fastcall Detour_WeaponEvaluateCandidate(uint32_t playerIndex, uint32_t weaponDatum, void* candidateDescriptor) {
    if (g_halo3Base && weaponDatum != 0xFFFFFFFF && weaponDatum != 0) {
        void* weaponObj = GetObjectPtr(g_halo3Base, weaponDatum);
        if (weaponObj) {
            uint32_t tagDatum = *(uint32_t*)weaponObj;
            uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
            void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
            const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

            LOG_ONCE_PER_DATUM(weapon_candidate, weaponDatum,
                "[weapon_candidate] Raycast evaluated weapon 0x%08X: name='%s', tag=0x%04X, unlocked=%d",
                weaponDatum, wname ? wname : "unknown", tagIndex, (int)IsTagUnlocked(g_halo3Base, tagIndex, tagDef, true));

            if (!IsTagUnlocked(g_halo3Base, tagIndex, tagDef, true)) {
                if (!ChiefHasAnyWeapon(g_halo3Base)) {
                    LOG_ONCE_PER_DATUM(unarmed_eval_bypass, weaponDatum,
                        "[weapon_candidate] Chief is unarmed! Allowing candidate '%s' (0x%04X) as emergency starter.",
                        wname ? wname : "unknown_weapon", tagIndex);
                } else {
                    return false; // Suppress candidate! Prompt will not show!
                }
            }
        }
    }
    return g_originalWeaponEvaluateCandidate ? g_originalWeaponEvaluateCandidate(playerIndex, weaponDatum, candidateDescriptor) : false;
}

// Detour for CanTradeWithAlly (+0x3578C8):
// Central evaluator for trading/equipping weapons.
// If the weapon being traded/acquired is locked, returning false completely prevents trade.
bool __fastcall Detour_CanTradeWithAlly(uint32_t unitDatum, uint32_t weaponDatum) {
    if (g_halo3Base && weaponDatum != 0 && weaponDatum != 0xFFFFFFFF) {
        void* weaponObj = GetObjectPtr(g_halo3Base, weaponDatum);
        if (weaponObj) {
            uint32_t tagDatum = *(uint32_t*)weaponObj;
            uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
            void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
            if (!IsTagUnlocked(g_halo3Base, tagIndex, tagDef, true)) {
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);
                if (!ChiefHasAnyWeapon(g_halo3Base)) {
                    LOG_ONCE_PER_DATUM(unarmed_trade_bypass, weaponDatum,
                        "[can_trade_with_ally] Chief is unarmed! Allowing trade '%s' (0x%04X) as emergency starter.",
                        wname ? wname : "unknown_weapon", tagIndex);
                } else {
                    LOG_ONCE_PER_DATUM(blocked_trade, weaponDatum,
                        "[can_trade_with_ally] BLOCKED trade/pickup of locked weapon '%s' (0x%04X, weapon=0x%08X) for unit 0x%08X",
                        wname ? wname : "unknown", tagIndex, weaponDatum, unitDatum);
                    return false; // Suppress candidate Action 2 from ever being created!
                }
            }
        }
    }
    return g_originalCanTradeWithAlly ? g_originalCanTradeWithAlly(unitDatum, weaponDatum) : false;
}

// Detour for raycast_evaluate_candidate (+0xEA4EC):
// Master crosshair raycast candidate evaluator called every frame when crosshair points at an object.
bool __fastcall Detour_RaycastEvaluateCandidate(uint32_t unitDatum, uint32_t targetDatum, void* queryContext) {
    if (g_halo3Base && targetDatum != 0xFFFFFFFF && targetDatum != 0) {
        if (unitDatum == 0 || IsPlayerUnit(g_halo3Base, unitDatum)) {
            void* targetObj = GetObjectPtr(g_halo3Base, targetDatum);
            if (targetObj) {
                uint32_t tagDatum = *(uint32_t*)targetObj;
                uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
                void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

                char group[5];
                ReadTagGroup(g_halo3Base, tagIndex, group);

                LOG_ONCE_PER_DATUM(aim_target, targetDatum,
                    "[aim_raycast] Chief aimed at target 0x%08X: group='%s', tagIndex=0x%04X, name='%s'",
                    targetDatum, group, tagIndex, wname ? wname : "non-weapon");
            }
        }
    }
    return g_originalRaycastEvaluateCandidate ? g_originalRaycastEvaluateCandidate(unitDatum, targetDatum, queryContext) : false;
}

// Detour for device_evaluate_candidate (+0xEA1A8):
// Evaluates devices, crates, and vehicles under crosshair.
bool __fastcall Detour_DeviceEvaluateCandidate(uint32_t unitDatum, uint32_t targetDatum, void* queryContext) {
    if (g_halo3Base && targetDatum != 0xFFFFFFFF && targetDatum != 0) {
        if (unitDatum == 0 || IsPlayerUnit(g_halo3Base, unitDatum)) {
            void* targetObj = GetObjectPtr(g_halo3Base, targetDatum);
            if (targetObj) {
                uint32_t tagDatum = *(uint32_t*)targetObj;
                uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
                void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

                char group[5];
                ReadTagGroup(g_halo3Base, tagIndex, group);

                LOG_ONCE_PER_DATUM(dev_target, targetDatum,
                    "[aim_device] Chief aimed at object 0x%08X: group='%s', tagIndex=0x%04X, name='%s'",
                    targetDatum, group, tagIndex, wname ? wname : "device/powerup");
            }
        }
    }
    return g_originalDeviceEvaluateCandidate ? g_originalDeviceEvaluateCandidate(unitDatum, targetDatum, queryContext) : false;
}

// Detour for UnitCanPickupWeapon (+0x35887C):
// Central weapon interactability evaluator.
bool __fastcall Detour_unit_can_pickup_weapon(uint32_t unitDatum, uint32_t weaponDatum, int slotPref, int* outSlot) {
    if (g_halo3Base && weaponDatum != 0xFFFFFFFF && weaponDatum != 0) {
        // ONLY gate the local player! NPCs (marines, grunts, brutes) must be free to equip all weapons!
        if (IsPlayerUnit(g_halo3Base, unitDatum)) {
            void* weaponObj = GetObjectPtr(g_halo3Base, weaponDatum);
            void* unitObj = GetObjectPtr(g_halo3Base, unitDatum);
            if (weaponObj) {
                uint32_t tagDatum = *(uint32_t*)weaponObj;
                uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
                void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

#if defined(_MSC_VER)
                void* retAddr = _ReturnAddress();
#else
                void* retAddr = __builtin_return_address(0);
#endif
                uintptr_t callerOffset = (uintptr_t)retAddr - g_halo3Base;
                // Callers that must pass through without gating locked weapons:
                // 1. Secondary inventory slot validity check (+0x0EAD0F)
                // 2. Engine gamestate serialization (+0x091F8C)
                // 3. unit_add_weapon internal validation (+0x3582DB; unit_add_weapon spans +0x358024..+0x358400)
                bool isBypassCaller = (callerOffset == 0x0EAD0F)
                                   || (callerOffset == 0x091F8C)
                                   || (callerOffset >= 0x358024 && callerOffset <= 0x358400);

                if (!IsTagUnlocked(g_halo3Base, tagIndex, tagDef, true)) {
                    if (isBypassCaller) {
                        LOG_ONCE_PER_DATUM(crate_bypass, weaponDatum,
                            "[can_pickup] Bypass pass-through: locked '%s' (0x%04X, weapon=0x%08X) at caller +0x%llX",
                            wname ? wname : "unknown", tagIndex, weaponDatum, (unsigned long long)callerOffset);
                    } else if (unitObj ? !UnitHasAnyWeapon(unitObj) : !ChiefHasAnyWeapon(g_halo3Base)) {
                        LOG_ONCE_PER_DATUM(unarmed_pickup_bypass, weaponDatum,
                            "[can_pickup] Chief is unarmed! Allowing '%s' (0x%04X, weapon=0x%08X) as emergency starter.",
                            wname ? wname : "unknown", tagIndex, weaponDatum);
                    } else {
                        LOG_ONCE_PER_DATUM(blocked_can_pickup, weaponDatum,
                            "[can_pickup] BLOCKED locked weapon '%s' (0x%04X, weapon=0x%08X) for player 0x%08X (caller=+0x%llX)",
                            wname ? wname : "unknown", tagIndex, weaponDatum, unitDatum, (unsigned long long)callerOffset);
                        return false;
                    }
                } else {
                    LOG_ONCE_PER_DATUM(allowed_can_pickup, weaponDatum,
                        "[can_pickup] ALLOWED unlocked weapon '%s' (0x%04X, weapon=0x%08X) for player 0x%08X (caller=+0x%llX)",
                        wname ? wname : "unlocked", tagIndex, weaponDatum, unitDatum, (unsigned long long)callerOffset);
                }
            }
        }
    }
    return g_originalUnitCanPickupWeapon ? g_originalUnitCanPickupWeapon(unitDatum, weaponDatum, slotPref, outSlot) : false;
}

// Detour for unit_update_weapon_prompt (+0x3B59EC):
// Evaluates and renders ground weapon pickup/swap HUD prompts.
bool __fastcall Detour_unit_update_weapon_prompt(uint32_t unitDatum, void* candidateData) {
    if (g_halo3Base && candidateData) {
        // [candidateData + 8] is the weapon datum index
        uint32_t weaponDatum = *(uint32_t*)((uintptr_t)candidateData + 0x8);
        if (weaponDatum != 0xFFFFFFFF && weaponDatum != 0) {
            void* weaponObj = GetObjectPtr(g_halo3Base, weaponDatum);
            if (weaponObj) {
                uint32_t tagDatum = *(uint32_t*)weaponObj;
                uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
                void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

                if (!IsTagUnlocked(g_halo3Base, tagIndex, tagDef, true)) {
                    void* unitObj = GetObjectPtr(g_halo3Base, unitDatum);
                    if (unitObj ? !UnitHasAnyWeapon(unitObj) : !ChiefHasAnyWeapon(g_halo3Base)) {
                        LOG_ONCE_PER_DATUM(unarmed_prompt_bypass, weaponDatum,
                            "[weapon_prompt] Chief is unarmed! Showing HUD prompt for '%s' (0x%04X, datum=0x%08X)",
                            wname ? wname : "unknown", tagIndex, weaponDatum);
                    } else {
                        LOG_ONCE_PER_DATUM(blocked_prompt, weaponDatum,
                            "[weapon_prompt] SUPPRESSED HUD prompt for locked '%s' (0x%04X, datum=0x%08X) for player 0x%08X",
                            wname ? wname : "unknown", tagIndex, weaponDatum, unitDatum);
                        return false; // Suppress prompt rendering completely!
                    }
                } else {
                    LOG_ONCE_PER_DATUM(allowed_prompt, weaponDatum,
                        "[weapon_prompt] ALLOWED HUD prompt for unlocked '%s' (0x%04X, datum=0x%08X) for player 0x%08X",
                        wname ? wname : "unlocked", tagIndex, weaponDatum, unitDatum);
                }
            }
        }
    }
    return g_originalUnitUpdateWeaponPrompt ? g_originalUnitUpdateWeaponPrompt(unitDatum, candidateData) : false;
}

// Detour for unit_action_swap_weapon (+0x3B2DAC):
// Manual weapon swap action executed when the player holds the interaction button.
bool __fastcall Detour_unit_action_swap_weapon(uint32_t unitDatum, void* interactionData) {
    if (g_originalUnitActionSwapWeapon) {
        return g_originalUnitActionSwapWeapon(unitDatum, interactionData);
    }
    return false;
}

// Detour for unit_action_pickup_weapon (+0x3B2F18):
// Proximity auto-pickup action function called when Chief is near a dropped weapon.
bool __fastcall Detour_unit_action_pickup_weapon(uint32_t unitDatum, void* interactionData) {
    if (g_halo3Base && interactionData) {
        // ONLY gate the local player! NPCs must be free to equip all weapons!
        if (!IsPlayerUnit(g_halo3Base, unitDatum)) {
            return g_originalUnitActionPickupWeapon ? g_originalUnitActionPickupWeapon(unitDatum, interactionData) : false;
        }

        // [interactionData + 4] is weaponDatum, [interactionData + 8] is slotPref
        uint32_t weaponDatum = *(uint32_t*)((uintptr_t)interactionData + 0x4);
        uint32_t slotPref = *(uint32_t*)((uintptr_t)interactionData + 0x8);
        if (weaponDatum != 0xFFFFFFFF && weaponDatum != 0) {
            void* weaponObj = GetObjectPtr(g_halo3Base, weaponDatum);
            if (weaponObj) {
                uint32_t tagDatum = *(uint32_t*)weaponObj;
                uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
                void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

                // 1. If weapon is locked, block auto-pickup unless Chief is unarmed
                if (!IsTagUnlocked(g_halo3Base, tagIndex, tagDef, true)) {
                    void* unitObj = GetObjectPtr(g_halo3Base, unitDatum);
                    if (unitObj ? !UnitHasAnyWeapon(unitObj) : !ChiefHasAnyWeapon(g_halo3Base)) {
                        LOG_ONCE_PER_DATUM(unarmed_action_pickup_bypass, weaponDatum,
                            "[action_pickup_weapon] Chief is unarmed! Allowing auto-pickup for '%s' (0x%04X, weapon=0x%08X) on player 0x%08X!",
                            wname ? wname : "unknown", tagIndex, weaponDatum, unitDatum);
                    } else {
                        LOG_ONCE_PER_DATUM(auto_blocked, weaponDatum,
                            "[action_pickup_weapon] SUPPRESSED auto-pickup for locked '%s' (0x%04X, weapon=0x%08X) on player 0x%08X!",
                            wname ? wname : "unknown", tagIndex, weaponDatum, unitDatum);
                        return false; // Suppress auto-pickup so reload and animations are never interrupted!
                    }
                }

                // 2. Unlocked weapons are allowed through to the original action handler

                LOG_ONCE_PER_DATUM(allowed_pickup, weaponDatum,
                    "[action_pickup_weapon] ALLOWED pickup action for unlocked '%s' (0x%04X, weapon=0x%08X, slotPref=%u) on player 0x%08X",
                    wname ? wname : "unlocked", tagIndex, weaponDatum, slotPref, unitDatum);
            }
        }
    }
    return g_originalUnitActionPickupWeapon ? g_originalUnitActionPickupWeapon(unitDatum, interactionData) : false;
}

// Detour for unit_add_weapon (+0x358024): Prevents player from receiving locked weapons from any source
bool __fastcall Detour_unit_add_weapon(uint32_t unitDatum, uint32_t weaponDatum, int slotPref) {
    if (g_halo3Base) {
        bool isPlayer = IsPlayerUnit(g_halo3Base, unitDatum);
        void* unitObj = isPlayer ? GetObjectPtr(g_halo3Base, unitDatum) : nullptr;

        if (isPlayer && unitObj) {
            void* weaponObj = GetObjectPtr(g_halo3Base, weaponDatum);
            if (weaponObj) {
                uint32_t tagDatum = *(uint32_t*)weaponObj;
                uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
                void* tagDef = GetTagDefinition(g_halo3Base, tagDatum);
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

                // Direct campaign skull check: if Chief equips a skull, award it immediately!
                CheckAndAwardSkullIfHolding(g_halo3Base, unitDatum, weaponObj, tagDef);

                if (!IsTagUnlocked(g_halo3Base, tagIndex, tagDef, true)) {
                    // Check if Chief is currently completely unarmed (e.g. initial spawn on The Ark / Crow's Nest)
                    bool chiefHasWeapon = UnitHasAnyWeapon(unitObj);
                    if (!chiefHasWeapon) {
                        Log("[unit_add_weapon] Chief is unarmed! Allowing '%s' (0x%04X) as emergency starter weapon.",
                            wname ? wname : "unknown_weapon", tagIndex);
                        // Allow this weapon so Chief is never left unarmed
                    } else {
#if defined(_MSC_VER)
                        void* caller = _ReturnAddress();
#else
                        void* caller = __builtin_return_address(0);
#endif
                        uintptr_t callerRva = (uintptr_t)caller - g_halo3Base;
                        LOG_ONCE_PER_DATUM(blocked_add, weaponDatum,
                            "[unit_add_weapon] BLOCKED locked weapon '%s' (0x%04X, tagDef=%p) from player 0x%08X (caller=halo3.dll+0x%X)!",
                            wname ? wname : "unknown_weapon", tagIndex, tagDef, unitDatum, (uint32_t)callerRva);
                        return false; // Prevent receiving locked weapon!
                    }
                } else {
#if defined(_MSC_VER)
                    void* caller = _ReturnAddress();
#else
                    void* caller = __builtin_return_address(0);
#endif
                    uintptr_t callerRva = (uintptr_t)caller - g_halo3Base;
                    LOG_ONCE_PER_DATUM(allowed_add, weaponDatum,
                        "[unit_add_weapon] ALLOWED unlocked weapon '%s' (0x%04X) to player 0x%08X (caller=halo3.dll+0x%X, slotPref=%d)",
                        wname ? wname : "unlocked", tagIndex, unitDatum, (uint32_t)callerRva, slotPref);
                }
            }
        }
    }
    return g_originalUnitAddWeapon ? g_originalUnitAddWeapon(unitDatum, weaponDatum, slotPref) : true;
}

} // namespace Halo3AP
