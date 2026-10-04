#pragma once
#include <cstdint>

namespace Halo3AP {

// ============================================================================
// EVENT DETECTION (Rally Points, Terminals, Skulls)
//
// These detect in-game events for Archipelago location checks:
// - Rally Point reached: Polled safely in Detour_Halo3EngineStep from TLS gameState + 0x16E
// - Terminal accessed: Detour on terminal_was_accessed (+0x1F7190)
// - Skull collected: Detours on campaign_metagame_award_primary/secondary_skull
//
// CALLING CONVENTION NOTES:
// The HS (Halo Script) evaluator at +0x1E1BAC dispatches to implementation
// - game_insertion_point_unlock: MOV ebx, edx => rally point index in edx (2nd param)
// - terminal_was_accessed:        MOV edi, edx => terminal index in edx (2nd param)
// - award_primary_skull:          MOV ebx, edx => skull index in edx (2nd param)
// - award_secondary_skull:        MOV ebx, edx => skull index in edx (2nd param)
// ============================================================================

void ResetEventState();
bool WasGameWonFired();

// --- Rally Point Detection ---
// game_insertion_point_unlock (halo3.dll + 0xF0810): rally index arrives in ecx.
// 0 = Alpha (start), 1 = Bravo, 2 = Charlie
typedef void (__fastcall *GameInsertionPointUnlock_t)(int32_t rallyIndex);
extern GameInsertionPointUnlock_t g_originalInsertionPointUnlock;
extern void*                      g_targetInsertionPointUnlock;
void __fastcall Detour_GameInsertionPointUnlock(int32_t rallyIndex);

// --- Terminal Detection ---
// terminal_activate (halo3.dll + 0x3A8798): native engine function called when terminal device is activated.
// Parameter: deviceDatum in ecx.
typedef void (__fastcall *TerminalActivate_t)(uint32_t deviceDatum);
extern TerminalActivate_t g_originalTerminalActivate;
extern void*              g_targetTerminalActivate;
void __fastcall Detour_TerminalActivate(uint32_t deviceDatum);

// Campaign terminal bitfield polling helper (runs on simulation thread)
void CheckPendingTerminals(uintptr_t halo3Base);

// --- Skull Detection (Primary: Iron, Black Eye, Tough Luck, etc.) ---
// campaign_metagame_award_primary_skull (halo3.dll + 0x136088): (playerDatum in ecx, skullIndex in edx)
typedef void (__fastcall *AwardPrimarySkull_t)(int32_t playerDatum, int32_t skullIndex);
extern AwardPrimarySkull_t g_originalAwardPrimarySkull;
extern void*               g_targetAwardPrimarySkull;
void __fastcall Detour_AwardPrimarySkull(int32_t playerDatum, int32_t skullIndex);

// --- Skull Detection (Secondary: Blind, Cowbell, IWHBYD, Grunt Birthday Party) ---
// campaign_metagame_award_secondary_skull (halo3.dll + 0x136580): (playerDatum in ecx, skullIndex in edx)
typedef void (__fastcall *AwardSecondarySkull_t)(int32_t playerDatum, int32_t skullIndex);
extern AwardSecondarySkull_t g_originalAwardSecondarySkull;
extern void*                 g_targetAwardSecondarySkull;
void __fastcall Detour_AwardSecondarySkull(int32_t playerDatum, int32_t skullIndex);

// --- Mission Completion Detection & Auto-Return-To-Menu ---
// game_won (+0x1F2F7C): HS impl called when a campaign mission is beaten.
// Our detour emits a "game_won" event and then calls game_save_and_quit to
// cleanly return to the main menu instead of letting MCC auto-advance the
// campaign playlist to the next level.
typedef void (__fastcall *GameWon_t)(void* rcxParam, int32_t edxParam);
extern GameWon_t g_originalGameWon;
extern void*     g_targetGameWon;
void __fastcall Detour_GameWon(void* rcxParam, int32_t edxParam);


// --- Load Next Campaign Level Hook (Blocks advancing playlist and forces clean Save & Quit to menu) ---
// load_next_campaign_level (halo3.dll + 0x1398E4): called when engine tries to advance campaign level.
// Intercepting this function blocks loading the next mission and directly calls request_save_and_quit (0xF9114).
typedef void (__fastcall *LoadNextCampaignLevel_t)(void* rcx, void* rdx, void* r8);
extern LoadNextCampaignLevel_t g_originalLoadNextCampaignLevel;
extern void*                  g_targetLoadNextCampaignLevel;
void __fastcall Detour_LoadNextCampaignLevel(void* rcx, void* rdx, void* r8);

} // namespace Halo3AP

