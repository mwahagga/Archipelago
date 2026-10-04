#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>

namespace Halo3AP {

// ============================================================================
// TAG SYSTEM — Halo 3 tag instance resolution and weapon identification
//
// Provides O(1) weapon identification via Set 0xE Switch-To StringIDs,
// dynamic tag table scanning, and weapon registry management.
// ============================================================================

/// Resolves the tag definition pointer from a 32-bit tag datum.
void* GetTagDefinition(uintptr_t halo3Base, uint32_t tagDatum);

/// Resolves a weapon's canonical name from its Switch-To StringID (Set 0xE).
/// Returns nullptr if the StringID doesn't match a known weapon.
const char* GetWeaponNameFromS11C(uint32_t s11c);

/// Resolves a weapon's canonical name from its tag index + tag definition.
/// Uses s11c for O(1) identification, falling back to the dynamic registry.
const char* GetWeaponNameFromTag(uint16_t tagIndex, void* tagDef);

/// Returns true if the weapon can be dual-wielded in Halo 3.
bool IsWeaponDualWieldableByName(const char* name);

/// Scans the full tag instance table to discover all weapon tag indices on
/// the current map. Called once on first simulation tick after each level load.
bool ScanAndRegisterMapWeapons(uintptr_t halo3Base);

/// Checks if a weapon tag is unlocked according to Archipelago state.
/// Non-weapon objects always return true. Unknown weapons default to unlocked.
bool IsTagUnlocked(uintptr_t halo3Base, uint16_t tagIndex, void* tagDef, bool isKnownWeaponContext = false);

/// Reads a tag group's 4-character code for a given tag index.
/// Writes into outGroup which must be at least 5 bytes (null-terminated).
void ReadTagGroup(uintptr_t halo3Base, uint16_t tagIndex, char outGroup[5]);

/// Resets the tag registry (called on map unload / hook removal).
void ResetTagRegistry();

} // namespace Halo3AP
