#pragma once
#include <cstdint>

namespace Halo3AP {

// ============================================================================
// ENGINE HOOKS — Halo 3 simulation tick and MCC dispatch hooks
// ============================================================================

/// Detour for Halo3EngineStep (+0xB2538): per-simulation-frame tick.
/// Triggers tag scanning on first tick and runs inventory enforcement.
void __fastcall Detour_Halo3EngineStep();

/// Detour for MCC Engine Dispatch (+0x42D6F8): main thread dispatch.
void* __fastcall Detour_MccEngineDispatch(void* a1, void* a2, void* a3, void* a4);

/// Detour for unit_get_seat_camera (+0x355E20): camera positioning for seated units.
/// Hardened against null parent vehicle objects during outro cutscenes (crash at +0x355EB9).
bool __fastcall Detour_UnitGetSeatCamera(uint32_t unitDatum, void* outTransform);

/// Detour for unit_update_sound (+0x1EC3C): updates sound emitters for units based on sound flags.
/// Detoured to silence the shield recharge audio loop (flag 0x80) when Chief's shields are at full unlocked cap (< 100%).
void __fastcall Detour_UnitUpdateSound(uint32_t unitDatum, uint32_t soundFlags);

// Original function pointer typedefs
typedef void (__fastcall *Halo3EngineStep_t)();
typedef void* (__fastcall *MccEngineDispatch_t)(void*, void*, void*, void*);
typedef bool (__fastcall *UnitGetSeatCamera_t)(uint32_t, void*);
typedef void (__fastcall *UnitUpdateSound_t)(uint32_t, uint32_t);

// Original function pointers (set by MinHook)
extern Halo3EngineStep_t   g_originalHalo3EngineStep;
extern MccEngineDispatch_t g_originalMccEngineDispatch;
extern UnitGetSeatCamera_t g_originalUnitGetSeatCamera;
extern UnitUpdateSound_t   g_originalUnitUpdateSound;

// Hook target pointers
extern void* g_targetHalo3EngineStep;
extern void* g_targetMccEngineDispatch;
extern void* g_targetUnitGetSeatCamera;
extern void* g_targetUnitUpdateSound;

} // namespace Halo3AP
