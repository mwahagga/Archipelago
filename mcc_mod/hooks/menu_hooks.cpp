#include "menu_hooks.h"
#include "lobby_hooks.h"
#include "../pattern_scan.h"
#include "../pipe_server.h"
#include "../memory_utils.h"
#include <MinHook.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace Halo3AP {

    namespace {
        const char* kAddItemPattern =
            "48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 57 48 83 ec 30 41 8b f0 48 8b ea 48 8b f9";

        // Mission carousel filtering hooks
        typedef bool (*AddItemFn)(void* widget, void* item, int param);
        AddItemFn g_addItemOriginal = nullptr;
        void* g_addItemTarget = nullptr;


        // =================================================================
        // AddItem Detour (hooks UIWidgetArray::AddItem at RVA +0x895F54)
        // =================================================================
        bool DetourAddItem(void* widget, void* item, int param) {
            uintptr_t exe = (uintptr_t)GetModuleHandleA(nullptr);
            uintptr_t missionItemVtable = exe + 0x329A270;
            uintptr_t missionGetIndexFn = exe + 0x28AEDC;

            uintptr_t vt = 0;
            void* fn = nullptr;
            if (item && IsMemoryReadable(item, sizeof(uintptr_t))) {
                vt = *(uintptr_t*)item;
                if (vt && IsMemoryReadable((void*)vt, 0x168)) {
                    fn = (void*)*(uintptr_t*)(vt + 0x160);
                }
            }

            // Strictly inspect items matching the MCC mission item vtable and function
            if (vt == missionItemVtable && (uintptr_t)fn == missionGetIndexFn) {
                if (IsMemoryReadable((uint8_t*)item + 0x16C, sizeof(uint16_t))) {
                    uint16_t missionIdx = *(uint16_t*)((uint8_t*)item + 0x16C);
                    std::string scen = MapMissionIndexToScenario(missionIdx);
                    Log("[MissionFilter] Mission card detected: idx=%u (%s), widget=%p, item=%p",
                        (unsigned int)missionIdx, scen.c_str(), widget, item);

                    // If this scenario is locked in Archipelago, DROP IT from the carousel!
                    if (IsClientConnected() && !scen.empty() && !IsMissionUnlocked(scen)) {
                        Log("[MissionFilter] >>> DROPPING LOCKED MISSION CARD: idx=%u (%s) <<<",
                            (unsigned int)missionIdx, scen.c_str());
                        return false; // Dropped from carousel widget!
                    }
                }
            }

            if (g_addItemOriginal) {
                return g_addItemOriginal(widget, item, param);
            }
            return false;
        }
    } // anonymous namespace

    bool InstallMenuHooks() {
        HMODULE exe = GetModuleHandleA(nullptr);

        // AddItem hook (UIWidgetArray::AddItem) - Mission carousel filtering
        g_addItemTarget = FindPatternInModule(exe, kAddItemPattern);
        if (!g_addItemTarget) {
            g_addItemTarget = (void*)((uintptr_t)exe + 0x895F54);
            Log("[MissionFilter] AddItem pattern not found, using RVA +0x895F54");
        }
        if (g_addItemTarget) {
            MH_STATUS status = MH_CreateHook(g_addItemTarget, (void*)&DetourAddItem, (void**)&g_addItemOriginal);
            if (status == MH_OK && MH_EnableHook(g_addItemTarget) == MH_OK) {
                Log("[MissionFilter] AddItem hook active at %p", g_addItemTarget);
            } else {
                Log("[MissionFilter] Failed to hook AddItem: %s", MH_StatusToString(status));
                g_addItemTarget = nullptr;
            }
        }

        return true;
    }

    void RemoveMenuHooks() {
        if (g_addItemTarget) {
            MH_DisableHook(g_addItemTarget);
            MH_RemoveHook(g_addItemTarget);
            g_addItemTarget = nullptr;
        }
        g_addItemOriginal = nullptr;
    }

} // namespace Halo3AP
