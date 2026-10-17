#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

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

/// Safely checks if an arbitrary memory region is committed and readable
/// using the OS memory manager (VirtualQuery), avoiding access violations.
inline bool IsMemoryReadable(const void* p, size_t size) {
    if (!p) return false;
    uintptr_t addr = (uintptr_t)p;
    if (addr < 0x10000 || addr >= 0x00007FFFFFFFFFFFULL || (addr + size) < addr) return false;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            uintptr_t pageEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            return (addr + size <= pageEnd);
        }
    }
    return false;
}

/// Dynamic function resolver: Searches a module for an ASCII string literal,
/// then finds a nearby LEA reg, [rip+disp32] reference in .text, and searches
/// forward up to searchWindow bytes for the registered function pointer or handler.
inline void* FindFunctionByStringXref(HMODULE hMod, const char* strLiteral, int searchWindow = 120) {
    if (!hMod || !strLiteral) return nullptr;

    auto dos = (IMAGE_DOS_HEADER*)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto nt = (IMAGE_NT_HEADERS*)((uint8_t*)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    uint8_t* base = (uint8_t*)hMod;
    size_t size = nt->OptionalHeader.SizeOfImage;
    size_t strLen = strlen(strLiteral);
    if (size < strLen + 7) return nullptr;

    // Step 1: Locate the ASCII string in module memory
    uint8_t* strAddr = nullptr;
    for (size_t i = 0; i <= size - strLen; ++i) {
        if (memcmp(base + i, strLiteral, strLen) == 0) {
            strAddr = base + i;
            break;
        }
    }
    if (!strAddr) return nullptr;

    // Step 2: Search for LEA reg64, [rip + disp32] referencing the string
    for (size_t i = 0; i <= size - 7; ++i) {
        // LEA r64, [rip + disp32]: 48 8D (ModRM & 0xC7 == 0x05) or 4C 8D (ModRM & 0xC7 == 0x05)
        if ((base[i] == 0x48 || base[i] == 0x4C) && base[i + 1] == 0x8D) {
            uint8_t modrm = base[i + 2];
            if ((modrm & 0xC7) != 0x05) continue;
            int32_t disp = *(int32_t*)(base + i + 3);
            uint8_t* target = base + i + 7 + disp;
            if (target == strAddr) {
                // Found reference to string; scan forward up to searchWindow bytes for a function LEA
                for (int j = 0; j < searchWindow; ++j) {
                    size_t pos = i + j;
                    if (pos + 7 >= size) break;
                    if ((base[pos] == 0x48 || base[pos] == 0x4C) && base[pos + 1] == 0x8D) {
                        uint8_t m = base[pos + 2];
                        if ((m & 0xC7) != 0x05) continue;
                        int32_t handlerDisp = *(int32_t*)(base + pos + 3);
                        void* handler = (void*)(base + pos + 7 + handlerDisp);
                        if (handler >= (void*)base && handler < (void*)(base + size) && handler != strAddr) {
                            return handler;
                        }
                    }
                }
            }
        }
    }
    return nullptr;
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
