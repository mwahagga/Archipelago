#include "tag_system.h"
#include "../memory_utils.h"
#include "../pipe_server.h"
#include "../offsets.h"
#include <unordered_set>

namespace Halo3AP {

// Dynamic weapon tag registry - populated by ScanAndRegisterMapWeapons() on level load
// Maps canonical weapon names (e.g. "assault_rifle") to/from map-specific tag indices
static std::mutex g_weaponTagMtx;
static std::unordered_map<std::string, uint16_t> g_weaponNameToTag;  // "assault_rifle" -> 0x1123
static std::unordered_map<uint16_t, std::string> g_tagToWeaponName;  // 0x1123 -> "assault_rifle"

void ResetTagRegistry() {
    std::lock_guard<std::mutex> lock(g_weaponTagMtx);
    g_weaponNameToTag.clear();
    g_tagToWeaponName.clear();
}

// Resolves the tag definition pointer from a 32-bit tag datum using Halo 3's native tag instance array
void* GetTagDefinition(uintptr_t halo3Base, uint32_t tagDatum) {
    if (!halo3Base || tagDatum == 0xFFFFFFFF || tagDatum == 0) return nullptr;

    uint16_t tagIndex = (uint16_t)(tagDatum & 0xFFFF);

    uintptr_t* pTagInstances = (uintptr_t*)(halo3Base + Halo3Offsets::H3_TAG_INSTANCES);
    if (!IsValidRead(pTagInstances, sizeof(uintptr_t))) return nullptr;
    uintptr_t tagInstances = *pTagInstances;
    if (!tagInstances || !IsValidRead((void*)tagInstances, 0x1000)) return nullptr;

    uint32_t* pOffsetDword = (uint32_t*)(tagInstances + (size_t)tagIndex * 8 + 4);
    if (!IsValidRead(pOffsetDword, sizeof(uint32_t))) return nullptr;
    uint32_t tagOffsetDword = *pOffsetDword;
    if (!tagOffsetDword) return nullptr;

    uintptr_t* pTagCacheBase = (uintptr_t*)(halo3Base + Halo3Offsets::H3_TAG_CACHE_BASE);
    if (!IsValidRead(pTagCacheBase, sizeof(uintptr_t))) return nullptr;
    uintptr_t tagCacheBase = *pTagCacheBase;
    if (!tagCacheBase || !IsValidRead((void*)tagCacheBase, 0x1000)) return nullptr;

    uintptr_t tagDefAddr = tagCacheBase + (uintptr_t)tagOffsetDword * 4;
    if (!IsValidRead((void*)tagDefAddr, 0x10)) return nullptr;

    return (void*)tagDefAddr;
}

// Resolves a weapon's canonical name from its tag definition's Switch-To StringID at +0x11C.
// Verified across all 9 Halo 3 campaign maps (010_jungle through 120_halo).
const char* GetWeaponNameFromS11C(uint32_t s11c) {
    switch (s11c) {
        case 0xE112: return "assault_rifle";
        case 0xE114: return "battle_rifle";
        case 0xE116: return "brute_shot";
        case 0xE117: return "covenant_carbine";
        case 0xE119: return "energy_sword";
        case 0xE11A: return "mauler";
        case 0xE11D: return "flamethrower";
        case 0xE11F: return "flak_cannon";
        case 0xE120: return "gravity_hammer";
        case 0xE122: return "machinegun_turret";
        case 0xE123: return "magnum";
        case 0xE124: return "missile_pod";
        case 0xE125: return "needler";
        case 0xE126: return "beam_rifle";
        case 0xE129: return "plasma_pistol";
        case 0xE12A: return "plasma_rifle";
        case 0xE12B: return "plasma_cannon";
        case 0xE12C: return "spartan_laser";
        case 0xE12D: return "rocket_launcher";
        case 0xE12E: return "sentinel_beam";
        case 0xE12F: return "shotgun";
        case 0xE130: return "smg";
        case 0xE131: return "sniper_rifle";
        case 0xE132: return "spiker";
        default: return nullptr;
    }
}

// Resolves a weapon's canonical name from its tag index + tag definition.
// Uses the s11c field at tagDef+0x11C for instant O(1) identification,
// falling back to the dynamic registry if the tag definition isn't available.
const char* GetWeaponNameFromTag(uint16_t tagIndex, void* tagDef) {
    // Primary path: read the s11c StringID directly from the tag definition (O(1), no lock)
    if (tagDef) {
        if (IsValidRead((void*)((uintptr_t)tagDef + 0x190), 2)) {
            uint16_t weaponType = *(uint16_t*)((uintptr_t)tagDef + 0x18C);
            // 11 = _weapon_type_ball (skulls and oddball). They are never locked Archipelago weapons.
            if (weaponType == 11) {
                return nullptr;
            }
        }
        if (IsValidRead((void*)((uintptr_t)tagDef + Halo3Offsets::H3_TAG_STRING_ID_S11C), 4)) {
            uint32_t s11c = *(uint32_t*)((uintptr_t)tagDef + Halo3Offsets::H3_TAG_STRING_ID_S11C);
            const char* name = GetWeaponNameFromS11C(s11c);
            if (name) return name;
        }
    }
    // Fallback: check dynamic registry (populated by ScanAndRegisterMapWeapons)
    {
        std::lock_guard<std::mutex> lock(g_weaponTagMtx);
        auto it = g_tagToWeaponName.find(tagIndex);
        if (it != g_tagToWeaponName.end()) {
            // Safe to return c_str() - the map is only modified during level load, not during gameplay
            return it->second.c_str();
        }
    }
    return nullptr;
}

// Returns true if the weapon can be dual-wielded in Halo 3
bool IsWeaponDualWieldableByName(const char* name) {
    if (!name) return false;
    if (strcmp(name, "magnum") == 0) return true;
    if (strcmp(name, "plasma_pistol") == 0) return true;
    if (strcmp(name, "smg") == 0) return true;
    if (strcmp(name, "plasma_rifle") == 0) return true;
    if (strcmp(name, "spiker") == 0) return true;
    if (strcmp(name, "mauler") == 0) return true;
    return false;
}

// Scans the full tag instance table to discover all weapon tag indices on the current map.
// Populates g_weaponNameToTag and g_tagToWeaponName for use by SpawnStartingWeaponForPlayer
// and all weapon gating hooks. Called once on first simulation tick after each level load.
bool ScanAndRegisterMapWeapons(uintptr_t halo3Base) {
    if (!halo3Base) return false;

    uintptr_t* pTagInstances = (uintptr_t*)(halo3Base + Halo3Offsets::H3_TAG_INSTANCES);
    if (!IsValidRead(pTagInstances, sizeof(uintptr_t))) return false;
    uintptr_t tagInstances = *pTagInstances;
    if (!tagInstances || !IsValidRead((void*)tagInstances, 0x1000)) return false;

    uintptr_t* pTagCacheBase = (uintptr_t*)(halo3Base + Halo3Offsets::H3_TAG_CACHE_BASE);
    if (!IsValidRead(pTagCacheBase, sizeof(uintptr_t))) return false;
    uintptr_t tagCacheBase = *pTagCacheBase;
    if (!tagCacheBase || !IsValidRead((void*)tagCacheBase, 0x1000)) return false;

    Log("[DynamicTagScan] Scanning tag table... tagInstances=0x%p, tagCacheBase=0x%p",
        (void*)tagInstances, (void*)tagCacheBase);

    std::lock_guard<std::mutex> lock(g_weaponTagMtx);
    g_weaponNameToTag.clear();
    g_tagToWeaponName.clear();

    int weaponsFound = 0;
    for (uint16_t tagIndex = 0; tagIndex < 8192; tagIndex++) {
        uintptr_t entryAddr = tagInstances + (size_t)tagIndex * 8 + 4;
        if (!IsValidRead((void*)entryAddr, sizeof(uint32_t))) break;
        uint32_t tagOffsetDword = *(uint32_t*)entryAddr;
        if (!tagOffsetDword || tagOffsetDword == 0xFFFFFFFF) continue;

        uintptr_t tagDefAddr = tagCacheBase + (uintptr_t)tagOffsetDword * 4;
        if (!IsValidRead((void*)tagDefAddr, 0x340)) continue;

        // Check object type == 2 (Weapon) at the start of the tag definition
        uint16_t objType = *(uint16_t*)tagDefAddr;
        if (objType != 0x0002) continue;

        // Skip skulls and oddballs (weapon type 11 = _weapon_type_ball)
        // so they are NEVER registered as SMG or any other weapon.
        uint16_t weaponType = *(uint16_t*)(tagDefAddr + 0x18C);
        if (weaponType == 11) {
            Log("[DynamicTagScan] Skipping skull/ball tagIndex 0x%04X (weaponType=%u)", tagIndex, (unsigned int)weaponType);
            continue;
        }

        // Read Switch-To StringID at +0x11C
        uint32_t s11c = *(uint32_t*)(tagDefAddr + 0x11C);
        const char* wname = GetWeaponNameFromS11C(s11c);
        if (wname) {
            g_weaponNameToTag[wname] = tagIndex;
            g_tagToWeaponName[tagIndex] = wname;
            weaponsFound++;
            Log("[DynamicTagScan]   %s -> tagIndex 0x%04X (s11c=0x%04X)", wname, tagIndex, s11c);
        }
    }

    Log("[DynamicTagScan] Scan complete: %d weapons registered on this map.", weaponsFound);
    return weaponsFound > 0;
}

bool IsTagUnlocked(uintptr_t halo3Base, uint16_t tagIndex, void* tagDef, bool isKnownWeaponContext) {
    if (!tagDef && halo3Base) {
        tagDef = GetTagDefinition(halo3Base, tagIndex);
    }

    // Explicitly verify if this tag is a skull or oddball (weapon type 11 = _weapon_type_ball, always unlocked)
    if (tagDef && IsValidRead(tagDef, 0x190)) {
        uint16_t weaponType = *(uint16_t*)((uintptr_t)tagDef + 0x18C);
        if (weaponType == 11) {
            return true;
        }
    }

    const char* wname = GetWeaponNameFromTag(tagIndex, tagDef);
    if (wname) {
        return IsWeaponUnlocked(wname);
    }
    // Any object with Object Type == 2 (Weapon) or checked in a weapon context
    // that is NOT in our recognized weapon table is an unknown weapon.
    // Unknown weapons are UNLOCKED by default, and logged once per tagIndex so we can track them.
    if (isKnownWeaponContext || (tagDef && *(uint16_t*)tagDef == 0x0002)) {
        static std::unordered_set<uint16_t> s_loggedUnknownTags;
        static std::mutex s_unknownMtx;
        bool isNew = false;
        {
            std::lock_guard<std::mutex> lock(s_unknownMtx);
            if (s_loggedUnknownTags.find(tagIndex) == s_loggedUnknownTags.end()) {
                s_loggedUnknownTags.insert(tagIndex);
                isNew = true;
            }
        }
        if (isNew) {
            uint32_t s11c = 0;
            if (tagDef) {
                s11c = *(uint32_t*)((uintptr_t)tagDef + Halo3Offsets::H3_TAG_STRING_ID_S11C);
            }
            Log("[WeaponGating] Unknown weapon tagIndex=0x%04X (s11c=0x%04X) - UNLOCKED by default.", tagIndex, s11c);
        }
        return true;
    }
    // Non-weapon objects (grenades, equipment, vehicles, scenery) are permitted
    return true;
}

void ReadTagGroup(uintptr_t halo3Base, uint16_t tagIndex, char outGroup[5]) {
    outGroup[0] = '?'; outGroup[1] = '?'; outGroup[2] = '?'; outGroup[3] = '?'; outGroup[4] = '\0';
    uintptr_t* pTagInstances = (uintptr_t*)(halo3Base + Halo3Offsets::H3_TAG_INSTANCES);
    if (IsValidRead(pTagInstances, sizeof(uintptr_t)) && *pTagInstances) {
        uintptr_t tagInstances = *pTagInstances;
        if (IsValidRead((void*)(tagInstances + (size_t)tagIndex * 8), 8)) {
            char* g = (char*)(tagInstances + (size_t)tagIndex * 8);
            outGroup[0] = g[0]; outGroup[1] = g[1]; outGroup[2] = g[2]; outGroup[3] = g[3]; outGroup[4] = '\0';
        }
    }
}

} // namespace Halo3AP
