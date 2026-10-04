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

/// Helper to extract selected chapter token from CampaignLobby struct.
std::string ExtractChapterFromLobby(void* lobby);

/// Maps menu chapter tokens (e.g. "$H3_Chapter_3-2_Title") to scenario IDs.
std::string MapChapterToScenario(const std::string& token);

// Original function pointer and target (managed by hook_manager)
typedef void (__fastcall *StartCountdown_t)(void* lobby, uint64_t mode);
extern StartCountdown_t g_originalStartCountdown;
extern void*            g_targetStartCountdown;

/// Detour for MCC RestartGameEngine (+0x1E6424). Suppresses auto-advancing playlist after mission complete.
void __fastcall Detour_RestartGameEngine(void* pThis, int32_t reason, void* r8, void* r9);
typedef void (__fastcall *RestartGameEngine_t)(void* pThis, int32_t reason, void* r8, void* r9);
extern RestartGameEngine_t g_originalRestartGameEngine;
extern void*               g_targetRestartGameEngine;

} // namespace Halo3AP
