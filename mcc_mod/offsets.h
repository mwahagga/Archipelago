#pragma once
#include <cstdint>

// ============================================================================
// HALO 3 / MCC REVERSE ENGINEERING MEMORY ADDRESSES & OFFSETS
//
// If MCC or halo3.dll is updated by a patch, all memory addresses detoured
// or read by the native mod can be maintained and updated here in one place.
// ============================================================================

namespace Halo3Offsets {
    // ------------------------------------------------------------------------
    // MCC-Win64-Shipping.exe Offsets
    // ------------------------------------------------------------------------
    // CampaignLobby::StartCountdown detour: clean convergence point for starting match
    constexpr uintptr_t MCC_START_COUNTDOWN     = 0x5137EC;

    // CampaignLobby::CommitSelection detour: intercepts mission and option selection commits
    constexpr uintptr_t MCC_COMMIT_SELECTION     = 0x50E1E4;
    constexpr uintptr_t MCC_GET_SELECTED_ITEM    = 0x4BC600;
    constexpr uint32_t  MCC_SCREEN_MISSION_SELECT = 0x6FDD6E37;

    // CampaignLobby level initialization & UI update
    constexpr uintptr_t MCC_INITIALIZE_LEVEL        = 0x509C24;
    constexpr uintptr_t MCC_UPDATE_SELECTED_MISSION = 0x50B2AC;

    // MCC Engine Dispatch: runs synchronously on the main simulation/render thread
    constexpr uintptr_t MCC_ENGINE_DISPATCH     = 0x42D6F8;

    // Scenario struct global pointer: *(uintptr_t*)(mccBase + MCC_SCENARIO_STRUCT)
    constexpr uintptr_t MCC_SCENARIO_STRUCT     = 0x03F8E778;
    constexpr uintptr_t MCC_SCENARIO_STRING     = 0x30;

    // Post-Game Carnage Report (PGCR) indicator & trigger
    constexpr uintptr_t MCC_PGCR_TRIGGER        = 0x4000B88; // 1 = trigger PGCR on restart
    constexpr uintptr_t MCC_PGCR_BASE           = 0x4000B8C;
    constexpr uintptr_t MCC_PGCR_OFFSET         = 0xC;

    // MCC RestartGameEngine: initiates engine reinitialization / loading next level in campaign playlist.
    // Detouring this function allows us to prevent advancing to the next mission after beating a level.
    // Signature: void __fastcall RestartGameEngine(void* pThis, int32_t reason, void* r8, void* r9)
    constexpr uintptr_t MCC_RESTART_GAME_ENGINE = 0x1E6424;
    constexpr uintptr_t MCC_RESTART_REASON      = 0x4000B70;



    // ------------------------------------------------------------------------
    // halo3.dll Offsets
    // ------------------------------------------------------------------------
    // weapon_evaluate_candidate detour (+0xEA6E4): real master interaction candidate evaluation for ground weapons.
    // Called every frame when looking at/near a dropped weapon. Returning false (0) prevents candidate registration,
    // suppresses HUD pickup/swap prompts, and prevents reload interruption.
    constexpr uintptr_t H3_WEAPON_EVALUATE_CANDIDATE       = 0xEA6E4;

    // biped_evaluate_candidate detour (+0x3B7438): master interaction candidate evaluation for bipeds.
    // Reserved for future use (e.g. skull/terminal detection). See reverse_engineering_notes.md §4 #5.
    constexpr uintptr_t H3_BIPED_EVALUATE_CANDIDATE        = 0x3B7438;

    // can_trade_with_ally detour (+0x3578C8): evaluates if Chief can trade weapons with an ally biped.
    // Called by raycast candidate evaluator (+0xEA9B3) and interaction dispatcher (+0x8BA3D, +0xEB2C4).
    // Returning false prevents ally trade candidate qualification, cleanly suppressing the "Hold [E] to trade" HUD prompt.
    constexpr uintptr_t H3_CAN_TRADE_WITH_ALLY            = 0x3578C8;

    // raycast_evaluate_candidate detour (+0xEA4EC): called when player's crosshair raycast intersects any object
    constexpr uintptr_t H3_RAYCAST_EVALUATE_CANDIDATE     = 0xEA4EC;

    // device_evaluate_candidate detour (+0xEA1A8): called when crosshair aims at devices, crates, or vehicles
    constexpr uintptr_t H3_DEVICE_EVALUATE_CANDIDATE      = 0xEA1A8;

    // unit_can_pickup_weapon detour (+0x35887C): universal interactability evaluator for weapons; returning false prevents raycast candidate qualification, prompts, and reload interruption
    constexpr uintptr_t H3_UNIT_CAN_PICKUP_WEAPON         = 0x35887C;

    // unit_update_weapon_prompt detour (+0x3B59EC): evaluates and renders ground weapon pickup/swap HUD prompts; returning false suppresses prompt
    constexpr uintptr_t H3_UNIT_UPDATE_WEAPON_PROMPT      = 0x3B59EC;

    // unit_update_swap_prompt detour (+0x3B62C0): evaluates and renders weapon swap & ally trade HUD prompts ("Hold [E] to swap / trade"); returning false suppresses prompt
    // Reserved for future use. Removed from active hooks per reverse_engineering_notes.md §1.2 #13.
    constexpr uintptr_t H3_UNIT_UPDATE_SWAP_PROMPT        = 0x3B62C0;

    // unit_update_trade_prompt detour (+0x3B6398): evaluates and renders secondary trade HUD prompt; returning false suppresses prompt
    // Reserved for future use. Removed from active hooks per reverse_engineering_notes.md §1.2 #10.
    constexpr uintptr_t H3_UNIT_UPDATE_TRADE_PROMPT       = 0x3B6398;

    // unit_find_interaction_candidate helper (+0x38DC80): searches player's interaction candidate table for given actionId (e.g. 6 for ally trade, 0x27 for weapon)
    // Reserved for future use. See reverse_engineering_notes.md §4 #3.
    constexpr uintptr_t H3_UNIT_FIND_INTERACTION_CANDIDATE = 0x38DC80;

    // chud_set_interaction_prompt detour (+0x382D88): HUD prompt display function; returning 0 prevents prompt rendering
    // Reserved for future use. Removed from active hooks per reverse_engineering_notes.md §1.2 #13.
    constexpr uintptr_t H3_CHUD_SET_INTERACTION_PROMPT     = 0x382D88;

    // unit_action_swap_weapon detour (+0x3B2DAC): manual weapon swap action executed on button hold
    constexpr uintptr_t H3_UNIT_ACTION_SWAP_WEAPON         = 0x3B2DAC;

    // Entity resolver helpers
    // Reserved for future use (e.g. skull/terminal/rally point object resolution).
    constexpr uintptr_t H3_RESOLVE_ENTITY                  = 0x52030;
    constexpr uintptr_t H3_RESOLVE_ENTITY_TYPED            = 0x52070;

    // unit_action_pickup_weapon detour: proximity auto-pickup action; returning false prevents reload interruption
    constexpr uintptr_t H3_UNIT_ACTION_PICKUP_WEAPON       = 0x3B2F18;

    // unit_add_weapon detour: blocks engine scripts / cutscenes from equipping locked weapons
    constexpr uintptr_t H3_UNIT_ADD_WEAPON      = 0x358024;

    // Halo3EngineStep detour: simulation tick at end of each simulation frame
    constexpr uintptr_t H3_ENGINE_STEP          = 0xB2538;

    // TLS index global pointer used to resolve the Halo 3 simulation thread data
    constexpr uintptr_t H3_TLS_INDEX            = 0xA39F9C;

    // Tag table pointers for dynamic weapon tag discovery
    constexpr uintptr_t H3_TAG_INSTANCES        = 0xA49018;
    constexpr uintptr_t H3_TAG_CACHE_BASE       = 0x1FCF4C8;

    // Weapon spawning and inventory management functions
    constexpr uintptr_t H3_UNIT_DROP_WEAPON     = 0x359FBC;
    constexpr uintptr_t H3_PLACEMENT_NEW        = 0x341508;
    constexpr uintptr_t H3_OBJECT_NEW           = 0x341AE8;
    constexpr uintptr_t H3_OBJECT_KILL          = 0x34D568;    // void __fastcall object_kill(uint32_t objectDatum)

    // Struct internal member offsets
    constexpr uintptr_t H3_UNIT_MAX_SHIELD      = 0xF0;    // Base maximum shield vitality (float)
    constexpr uintptr_t H3_UNIT_HEALTH          = 0xF4;    // Current unit health (float)
    constexpr uintptr_t H3_UNIT_SHIELD          = 0xF8;    // Current unit shield vitality (float)
    constexpr uintptr_t H3_UNIT_RECHARGE_DELAY  = 0x10C;   // 16-bit shield recharge delay timer; 0x7FFF = paused/full
    constexpr uintptr_t H3_UNIT_FLAGS           = 0x110;   // Unit status bitflags; bit 10 (0x400) = recharge sound loop
    constexpr uintptr_t H3_UNIT_INVENTORY       = 0x268;   // 4 slots of 4 bytes each
    constexpr uintptr_t H3_TAG_STRING_ID_S11C   = 0x11C;   // Switch-To StringID in tag definition

    // Overshield decay branch patch (+0x36E5FA): changes 'jbe +0x74' (0x76 0x74) to 'jmp +0x74' (0xEB 0x74).
    // Bypasses the campaign engine's automatic decay of shields above 1.5x, allowing permanent overshields up to 3.0x.
    constexpr uintptr_t H3_OVERSHIELD_DECAY_PATCH        = 0x36E5FA;

    // unit_update_sound detour (+0x1EC3C): updates sound emitters for units based on sound flags.
    // Detoured to silence the shield recharge audio loop (flag 0x80) when Chief's shields are at full unlocked cap (< 100%).
    constexpr uintptr_t H3_UNIT_UPDATE_SOUND             = 0x1EC3C;

    // ------------------------------------------------------------------------
    // Event Detection (Rally Points, Terminals, Skulls)
    // ------------------------------------------------------------------------
    // game_insertion_point_unlock: called by engine when unlocking rally points.
    // Index arrives in ecx (1 = Bravo, 2 = Charlie).
    constexpr uintptr_t H3_INSERTION_POINT_UNLOCK        = 0xF0810;

    // terminal_activate: native engine function called when terminal device is activated.
    // Parameter: deviceDatum in ecx.
    constexpr uintptr_t H3_TERMINAL_ACTIVATE             = 0x3A8798;
    constexpr uintptr_t H3_TERMINAL_WAS_ACCESSED         = H3_TERMINAL_ACTIVATE;
    constexpr uintptr_t H3_CAMPAIGN_ACCESSED_TERMINALS   = 0x132;
    constexpr uintptr_t H3_CAMPAIGN_READING_TERMINAL     = 0x134;

    // campaign_metagame_award_primary_skull: awards a primary skull on pickup.
    // Signature: void __fastcall award_primary_skull(int32_t playerDatum, int32_t skullIndex)
    constexpr uintptr_t H3_AWARD_PRIMARY_SKULL           = 0x136088;

    // campaign_metagame_award_secondary_skull: awards a secondary skull on pickup.
    // Signature: void __fastcall award_secondary_skull(int32_t playerDatum, int32_t skullIndex)
    constexpr uintptr_t H3_AWARD_SECONDARY_SKULL         = 0x136580;

    // game_won: HS impl called when a campaign mission is beaten.
    // Signature: void __fastcall game_won(void* rcxParam, int32_t edxParam)
    // Prologue: 40 53 48 83 EC 20 8B DA (push rbx; sub rsp,20h; mov ebx,edx)
    constexpr uintptr_t H3_GAME_WON                      = 0x1F2F7C;

    // game_save_and_quit: HS impl that saves and cleanly exits to main menu.
    // Signature: void __fastcall game_save_and_quit(void* rcxParam, int32_t edxParam)
    // Prologue: 8B CA 33 D2 E9 ... (mov ecx,edx; xor edx,edx; jmp ...)
    constexpr uintptr_t H3_GAME_SAVE_AND_QUIT            = 0x1065F4;

    // game_won branch patch (+0xF05A3): changes 'jne +0x2A' (0x75 0x2A) to 'jmp +0x2A' (0xEB 0x2A).
    // In game_won handler (0xF053C), this bypasses LoadNextCampaignLevel (+0x1398E4) and immediately
    // branches to the session conclusion / return to lobby routine (+0xF05CF).
    // This avoids hooking the global 0xF01A0 query (which has 150 callers across the engine and crashes
    // at 0x132140 if caller-saved registers like r9 are clobbered by detours).
    constexpr uintptr_t H3_GAME_WON_SKIP_NEXT_LEVEL_PATCH = 0xF05A3;

    // load_next_campaign_level: called by 0xF05CA to advance to the next campaign mission after game_won.
    // Detouring this function blocks Halo 3 from preparing the next campaign level.
    constexpr uintptr_t H3_LOAD_NEXT_CAMPAIGN_LEVEL      = 0x1398E4;

    // unit_get_seat_camera (+0x355E20): camera positioning for seated/parented units.
    // Detoured to guard against unhandled null parent vehicle pointers during outro cutscenes (crash at +0x355EB9).
    constexpr uintptr_t H3_UNIT_GET_SEAT_CAMERA          = 0x355E20;

    // Halo 3 campaign level advance patch (+0x12E5AB):
    // In Halo 3 engine main loop (+0x12E584), when a campaign mission finishes and outro cutscene ends,
    // this instruction 'je +0x38' (0x74 0x38) branches to +0x12E5E5 which invokes +0x12EBC4 to load
    // the next campaign mission (e.g. Crow's Nest). NOP'ing this branch (0x90 0x90) causes the engine
    // to fall through to +0x12E5AD (the campaign completion / return to menu routine), cleanly
    // returning the player to the MCC menu / PGCR without advancing to the next level.
    constexpr uintptr_t H3_CAMPAIGN_LEVEL_ADVANCE_PATCH  = 0x12E5AB;

    // main_game_load_next_level (+0x12EBC4): the core internal function that logs 'level completed'
    // and triggers map transition. Patched to return false immediately as defense-in-depth.
    constexpr uintptr_t H3_MAIN_GAME_LOAD_NEXT_LEVEL     = 0x12EBC4;

    // ------------------------------------------------------------------------
    // Campaign Skull Eligibility & Spawning Patches
    // ------------------------------------------------------------------------
    // 1. Easy difficulty eligibility check bypass (+0x12FF57):
    //    'jl +0x9C' (0x0F 0x8C 0x9C 0x00 0x00 0x00) -> 6x NOP (0x90).
    //    Allows campaign skulls to be unlocked/awarded on Easy difficulty (difficulty 0).
    constexpr uintptr_t H3_SKULL_EASY_DIFFICULTY_PATCH      = 0x12FF57;

    // 2. Mid-mission insertion point check bypass (+0x12FFCC):
    //    'test r8w, r8w' (0x66 0x45 0x85 0xC0) -> 'xor r8w, r8w' (0x66 0x45 0x31 0xC0).
    //    Clears insertion index to 0 for skull eligibility so Rally Points Bravo and Charlie qualify.
    constexpr uintptr_t H3_SKULL_INSERTION_POINT_PATCH      = 0x12FFCC;

    // 3. Metagame difficulty bypass (+0x13627F):
    //    'jle +0x6B' (0x7E 0x6B) -> 2x NOP (0x90 0x90).
    //    Allows the metagame skull progression handler (+0x136248) to process skulls on Easy difficulty.
    constexpr uintptr_t H3_METAGAME_SKULL_DIFFICULTY_PATCH  = 0x13627F;

    // 4. Mission completion insertion check bypass (+0xF03CA):
    //    'jne +0xAC' (0x0F 0x85 0xAC 0x00 0x00 0x00) -> 6x NOP (0x90).
    //    Ensures the end-of-mission handler (+0xF0268) processes skull completion even when starting from mid-mission rally points.
    constexpr uintptr_t H3_MISSION_COMPLETE_INSERTION_PATCH = 0xF03CA;
}

