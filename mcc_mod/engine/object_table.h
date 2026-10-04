#pragma once
#include <cstdint>
#include <atomic>

namespace Halo3AP {

// ============================================================================
// OBJECT TABLE — Halo 3 object datum resolution and player identification
//
// Resolves Blam datum indices to raw memory pointers using TLS-based object
// tables. Provides player identification and inventory enforcement.
// ============================================================================

/// Resolves an object datum index to its raw memory pointer in halo3.dll.
/// Falls back to cached object table data when called from non-simulation threads.
void* GetObjectPtr(uintptr_t halo3Base, uint32_t datum);

/// Returns true if the given unit datum belongs to the local player (Master Chief).
bool IsPlayerUnit(uintptr_t halo3Base, uint32_t unitDatum);

/// Runs per-tick inventory enforcement: strips locked weapons from Chief's
/// inventory while preserving the "last weapon" safety net.
void CheckAndEnforcePlayerInventory(uintptr_t halo3Base);

/// Reads gs:[0x58] for thread-local storage access. Only valid on the
/// Halo 3 simulation thread.
uint64_t ReadGs58();

/// Cached player datum — updated each simulation tick.
extern std::atomic<uint32_t> g_localPlayerDatum;

/// Cached object table data pointer for cross-thread GetObjectPtr calls.
extern uintptr_t g_cachedObjectTableData;

} // namespace Halo3AP
