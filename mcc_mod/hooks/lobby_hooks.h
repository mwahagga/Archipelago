#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <string>

namespace Halo3AP {

// ============================================================================
// LOBBY HOOKS — MCC CampaignLobby interception for mission gating
// ============================================================================

/// Detour for CampaignLobby::StartCountdown. Blocks locked missions.
void __fastcall Detour_StartCountdown(void* lobby, uint64_t mode);

/// Detour for CampaignLobby::CommitSelection (+0x50E1E4). Blocks selecting locked missions.
void __fastcall Detour_CommitSelection(void* lobby);

/// Detour for CampaignLobby::InitializeLevel (+0x509C24). Ensures lobby preselects an unlocked mission.
void __fastcall Detour_InitializeLevel(void* lobby, void* rdx);

/// Helper to extract selected chapter token from CampaignLobby struct.
std::string ExtractChapterFromLobby(void* lobby);

/// Maps menu chapter tokens (e.g. "$H3_Chapter_3-2_Title") to scenario IDs.
std::string MapChapterToScenario(const std::string& token);

/// Helper to get the clicked mission index from CampaignLobby Scaleform list.
uint16_t GetClickedMissionIndex(void* lobby);

/// Maps numeric mission index (0..10) to scenario ID string.
std::string MapMissionIndexToScenario(uint16_t index);

/// Returns the index (1..9) of the first available unlocked mission in Archipelago.
uint16_t GetFirstUnlockedMissionIndex();

// Original function pointer and target (managed by hook_manager)
typedef void (__fastcall *StartCountdown_t)(void* lobby, uint64_t mode);
extern StartCountdown_t g_originalStartCountdown;
extern void*            g_targetStartCountdown;

typedef void (__fastcall *CommitSelection_t)(void* lobby);
extern CommitSelection_t g_originalCommitSelection;
extern void*            g_targetCommitSelection;

typedef void (__fastcall *InitializeLevel_t)(void* lobby, void* rdx);
extern InitializeLevel_t g_originalInitializeLevel;
extern void*             g_targetInitializeLevel;

/// Detour for MCC RestartGameEngine (+0x1E6424). Suppresses auto-advancing playlist after mission complete.
void __fastcall Detour_RestartGameEngine(void* pThis, int32_t reason, void* r8, void* r9);
typedef void (__fastcall *RestartGameEngine_t)(void* pThis, int32_t reason, void* r8, void* r9);
extern RestartGameEngine_t g_originalRestartGameEngine;
extern void*               g_targetRestartGameEngine;

} // namespace Halo3AP
