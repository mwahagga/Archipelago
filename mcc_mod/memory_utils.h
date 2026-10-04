#pragma once
#include <cstdint>
#include <cstddef>

// ============================================================================
// SHARED MEMORY UTILITIES
//
// Common inline helpers used across hook_manager, pipe_server, and engine
// subsystems. Extracted to eliminate duplication (formerly duplicated 9x
// as local lambdas throughout hook_manager.cpp).
// ============================================================================

namespace Halo3AP {

/// Validates that a pointer range falls within legal user-mode 64-bit address
/// space before dereferencing. Used for all object/tag/TLS pointer chains.
inline bool IsValidRead(const void* p, size_t size) {
    uintptr_t v = (uintptr_t)p;
    return (v > 0x10000 && v < 0x00007FFFFFFFFFFFULL && (v + size) > v);
}

} // namespace Halo3AP

// ============================================================================
// LOG_ONCE_PER_DATUM — Deduplicated per-datum logging macro
//
// Many detour hooks need to log the first time a specific datum is encountered
// without spamming every frame. This macro encapsulates the static-last-datum
// pattern that was repeated ~15 times across hook_manager.cpp.
//
// Usage:
//   LOG_ONCE_PER_DATUM(blocked_trade, weaponDatum,
//       "[can_trade] BLOCKED weapon 0x%08X", weaponDatum);
// ============================================================================
#define LOG_ONCE_PER_DATUM(tag, datum, fmt, ...) do { \
    static uint32_t s_lastLogged_##tag = 0; \
    if (s_lastLogged_##tag != (datum)) { \
        s_lastLogged_##tag = (datum); \
        ::Halo3AP::Log(fmt, ##__VA_ARGS__); \
    } \
} while(0)
