#pragma once
#include <cstdint>

namespace Halo3AP {

// ============================================================================
// WEAPON HOOKS — Halo 3 weapon interaction gating for Archipelago
//
// All detours that prevent picking up, swapping, or trading locked weapons.
// Each detour hooks a specific point in the Halo 3 interaction pipeline
// to ensure locked weapons are never registered as interaction candidates.
// ============================================================================

// Detour function signatures (called by MinHook trampolines)
bool __fastcall Detour_WeaponEvaluateCandidate(uint32_t playerIndex, uint32_t weaponDatum, void* candidateDescriptor);
bool __fastcall Detour_CanTradeWithAlly(uint32_t unitDatum, uint32_t weaponDatum);
bool __fastcall Detour_RaycastEvaluateCandidate(uint32_t unitDatum, uint32_t targetDatum, void* queryContext);
bool __fastcall Detour_DeviceEvaluateCandidate(uint32_t unitDatum, uint32_t targetDatum, void* queryContext);
bool __fastcall Detour_unit_can_pickup_weapon(uint32_t unitDatum, uint32_t weaponDatum, int slotPref, int* outSlot);
bool __fastcall Detour_unit_update_weapon_prompt(uint32_t unitDatum, void* candidateData);
bool __fastcall Detour_unit_action_swap_weapon(uint32_t unitDatum, void* interactionData);
bool __fastcall Detour_unit_action_pickup_weapon(uint32_t unitDatum, void* interactionData);
bool __fastcall Detour_unit_add_weapon(uint32_t unitDatum, uint32_t weaponDatum, int slotPref);

// Original function pointer typedefs
typedef bool (__fastcall *WeaponEvaluateCandidate_t)(uint32_t, uint32_t, void*);
typedef bool (__fastcall *CanTradeWithAlly_t)(uint32_t, uint32_t);
typedef bool (__fastcall *RaycastEvaluateCandidate_t)(uint32_t, uint32_t, void*);
typedef bool (__fastcall *DeviceEvaluateCandidate_t)(uint32_t, uint32_t, void*);
typedef bool (__fastcall *UnitCanPickupWeapon_t)(uint32_t, uint32_t, int, int*);
typedef bool (__fastcall *UnitUpdateWeaponPrompt_t)(uint32_t, void*);
typedef bool (__fastcall *UnitActionSwapWeapon_t)(uint32_t, void*);
typedef bool (__fastcall *UnitActionPickupWeapon_t)(uint32_t, void*);
typedef bool (__fastcall *UnitAddWeapon_t)(uint32_t, uint32_t, int);

// Original function pointers (set by MinHook)
extern WeaponEvaluateCandidate_t  g_originalWeaponEvaluateCandidate;
extern CanTradeWithAlly_t         g_originalCanTradeWithAlly;
extern RaycastEvaluateCandidate_t g_originalRaycastEvaluateCandidate;
extern DeviceEvaluateCandidate_t  g_originalDeviceEvaluateCandidate;
extern UnitCanPickupWeapon_t      g_originalUnitCanPickupWeapon;
extern UnitUpdateWeaponPrompt_t   g_originalUnitUpdateWeaponPrompt;
extern UnitActionSwapWeapon_t     g_originalUnitActionSwapWeapon;
extern UnitActionPickupWeapon_t   g_originalUnitActionPickupWeapon;
extern UnitAddWeapon_t            g_originalUnitAddWeapon;

// Hook target pointers (for MH_CreateHook / MH_RemoveHook)
extern void* g_targetWeaponEvaluateCandidate;
extern void* g_targetCanTradeWithAlly;
extern void* g_targetRaycastEvaluateCandidate;
extern void* g_targetDeviceEvaluateCandidate;
extern void* g_targetUnitCanPickupWeapon;
extern void* g_targetUnitUpdateWeaponPrompt;
extern void* g_targetUnitActionSwapWeapon;
extern void* g_targetUnitActionPickupWeapon;
extern void* g_targetUnitAddWeapon;

} // namespace Halo3AP
