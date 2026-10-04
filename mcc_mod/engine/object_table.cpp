#include "object_table.h"
#include "tag_system.h"
#include "../memory_utils.h"
#include "../pipe_server.h"
#include "../offsets.h"

namespace Halo3AP {

std::atomic<uint32_t> g_localPlayerDatum(0xFFFFFFFF);
uintptr_t g_cachedObjectTableData = 0;

uint64_t ReadGs58() {
    uint64_t val = 0;
#if defined(_MSC_VER)
    val = __readgsqword(0x58);
#elif defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("movq %%gs:0x58, %0" : "=r"(val));
#endif
    return val;
}

// Resolves an object datum index to its raw memory pointer in halo3.dll
void* GetObjectPtr(uintptr_t halo3Base, uint32_t datum) {
    if (datum == 0xFFFFFFFF || datum == 0) return nullptr;

    uintptr_t tableData = 0;

    uint32_t* pTlsIndex = (uint32_t*)(halo3Base + Halo3Offsets::H3_TLS_INDEX);
    if (pTlsIndex && IsValidRead(pTlsIndex, sizeof(uint32_t))) {
        uint32_t tlsIndex = *pTlsIndex;
        if (tlsIndex < 128) {
            uint64_t gs58 = ReadGs58();
            if (gs58) {
                uint64_t* tlsArray = (uint64_t*)gs58;
                if (IsValidRead(&tlsArray[tlsIndex], sizeof(uint64_t))) {
                    uintptr_t threadData = tlsArray[tlsIndex];
                    if (threadData && IsValidRead((void*)threadData, 0x100)) {
                        uintptr_t tableHeader = *(uintptr_t*)(threadData + 0x38);
                        if (tableHeader && IsValidRead((void*)tableHeader, 0x50)) {
                            tableData = *(uintptr_t*)(tableHeader + 0x48);
                        }
                    }
                }
            }
        }
    }

    // If calling from a non-simulation thread (e.g. render/HUD thread), fall back to cached table data
    if (!tableData && g_cachedObjectTableData) {
        tableData = g_cachedObjectTableData;
    }

    if (!tableData || !IsValidRead((void*)tableData, 0x50)) return nullptr;

    // In Blam/Halo datum indices, low 16 bits is the direct table slot index
    uint16_t index = (uint16_t)(datum & 0xFFFF);
    size_t elementOffset = 0x10 + (size_t)index * 24;
    if (!IsValidRead((void*)(tableData + elementOffset), sizeof(void*))) return nullptr;

    void* objectPtr = *(void**)(tableData + elementOffset);
    if (!objectPtr || !IsValidRead(objectPtr, 0x180)) return nullptr;
    return objectPtr;
}

// Resolves whether a given unit datum belongs to the local player (Master Chief)
bool IsPlayerUnit(uintptr_t halo3Base, uint32_t unitDatum) {
    if (unitDatum == 0xFFFFFFFF || unitDatum == 0) return false;
    uint32_t chief = g_localPlayerDatum.load();
    if (chief != 0xFFFFFFFF && chief != 0) {
        return (unitDatum == chief);
    }
    // Dynamic fallback to thread-local storage player table
    if (!halo3Base) return false;
    uint32_t* pTlsIndex = (uint32_t*)(halo3Base + Halo3Offsets::H3_TLS_INDEX);
    if (!pTlsIndex || !IsValidRead(pTlsIndex, sizeof(uint32_t))) return false;
    uint32_t tlsIndex = *pTlsIndex;
    if (tlsIndex >= 128) return false;
    uint64_t gs58 = ReadGs58();
    uint64_t* tlsArray = (uint64_t*)gs58;
    if (!tlsArray || !IsValidRead(&tlsArray[tlsIndex], sizeof(uint64_t))) return false;
    uintptr_t threadData = tlsArray[tlsIndex];
    if (!threadData || !IsValidRead((void*)threadData, 0x100)) return false;
    uintptr_t playerTable = *(uintptr_t*)(threadData + 0x10);
    if (!playerTable || !IsValidRead((void*)playerTable, 0x50)) return false;
    uintptr_t tableData = *(uintptr_t*)(playerTable + 0x48);
    if (!tableData || !IsValidRead((void*)tableData, 0x50)) return false;
    chief = *(uint32_t*)(tableData + 0x28);
    if (chief != 0xFFFFFFFF && chief != 0) {
        g_localPlayerDatum.store(chief);
        return (unitDatum == chief);
    }
    return false;
}

void CheckAndEnforcePlayerInventory(uintptr_t halo3Base) {
    if (!halo3Base) return;

    uint32_t* pTlsIndex = (uint32_t*)(halo3Base + Halo3Offsets::H3_TLS_INDEX);
    uint32_t tlsIndex = (pTlsIndex && IsValidRead(pTlsIndex, sizeof(uint32_t))) ? *pTlsIndex : 999;
    uint64_t gs58 = ReadGs58();
    uint64_t* tlsArray = (uint64_t*)gs58;
    uintptr_t threadData = (tlsArray && tlsIndex < 128 && IsValidRead(&tlsArray[tlsIndex], sizeof(uint64_t))) ? tlsArray[tlsIndex] : 0;

    uintptr_t playerTable = (threadData && IsValidRead((void*)threadData, 0x100)) ? *(uintptr_t*)(threadData + 0x10) : 0;
    uintptr_t tableData = (playerTable && IsValidRead((void*)playerTable, 0x50)) ? *(uintptr_t*)(playerTable + 0x48) : 0;
    uint32_t chiefDatum = (tableData && IsValidRead((void*)tableData, 0x50)) ? *(uint32_t*)(tableData + 0x28) : 0;
    if (chiefDatum != 0xFFFFFFFF && chiefDatum != 0) {
        g_localPlayerDatum.store(chiefDatum);
    }

    // Cache the master object table pointer (at threadData + 0x38) for cross-thread GetObjectPtr calls
    uintptr_t objHeader = (threadData && IsValidRead((void*)threadData, 0x100)) ? *(uintptr_t*)(threadData + 0x38) : 0;
    uintptr_t objTableData = (objHeader && IsValidRead((void*)objHeader, 0x50)) ? *(uintptr_t*)(objHeader + 0x48) : 0;
    if (objTableData && IsValidRead((void*)objTableData, 0x50)) {
        g_cachedObjectTableData = objTableData;
    }

    if (!threadData || !IsValidRead((void*)threadData, 0x100)) return;
    if (!playerTable || !IsValidRead((void*)playerTable, 0x50)) return;
    if (!tableData || !IsValidRead((void*)tableData, 0x50)) return;
    if (chiefDatum == 0xFFFFFFFF || chiefDatum == 0) return;

    void* chiefObj = GetObjectPtr(halo3Base, chiefDatum);
    if (!chiefObj || !IsValidRead(chiefObj, 0x280)) return;

    // Chief inventory is at +0x268 (4 slots of 4 bytes each)
    uint32_t* inv = (uint32_t*)((uintptr_t)chiefObj + Halo3Offsets::H3_UNIT_INVENTORY);
    typedef void (__fastcall *UnitDropWeapon_t)(uint32_t unitDatum, uint32_t weaponDatum);
    auto UnitDropWeapon = (UnitDropWeapon_t)(halo3Base + Halo3Offsets::H3_UNIT_DROP_WEAPON);

    // 1. Count valid weapons currently in inventory
    int totalWeapons = 0;
    for (int slot = 0; slot < 4; ++slot) {
        uint32_t wDatum = inv[slot];
        if (wDatum != 0xFFFFFFFF && wDatum != 0) {
            totalWeapons++;
        }
    }

    // 2. Strip locked weapons, but apply the "Last Weapon" rule:
    // If Chief only has 1 weapon remaining, NEVER strip it so he is never left empty-handed.
    for (int slot = 0; slot < 4; ++slot) {
        uint32_t wDatum = inv[slot];
        if (wDatum != 0xFFFFFFFF && wDatum != 0) {
            void* wObj = GetObjectPtr(halo3Base, wDatum);
            if (wObj && IsValidRead(wObj, 0x10)) {
                uint32_t tagDatum = *(uint32_t*)wObj;
                uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);
                void* tagDef = GetTagDefinition(halo3Base, tagDatum);
                const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);

                bool unlocked = IsTagUnlocked(halo3Base, tagIndex, tagDef, true);

                if (!unlocked) {
                    if (totalWeapons > 1) {
                        Log("[CheckInventory] STRIPPING locked weapon '%s' (tagIndex=0x%04X, tagDef=%p) from Chief slot %d!",
                            wname ? wname : "unknown_weapon", tagIndex, tagDef, slot);
                        UnitDropWeapon(chiefDatum, wDatum);
                        totalWeapons--;
                    } else {
                        // Chief has only 1 weapon remaining. Keep it to preserve the unarmed safety net!
                    }
                }
            }
        }
    }
}

} // namespace Halo3AP
