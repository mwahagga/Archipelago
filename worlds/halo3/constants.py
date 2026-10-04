from __future__ import annotations

# ============================================================================
# HALO 3 CONSTANTS & MEMORY ADDRESS REGISTRY
# ============================================================================

# Canonical Halo 3 Campaign Missions in chronological order
MISSION_NAMES: list[str] = [
    "Sierra 117",
    "Crow's Nest",
    "Tsavo Highway",
    "The Storm",
    "Floodgate",
    "The Ark",
    "The Covenant",
    "Cortana",
    "Halo",
]

# Scenario code to friendly mission name mapping.
# These codes correspond exactly to Halo 3 engine map identifiers
# (as confirmed by the native mod's pipe_server and hook_manager).
SCENARIO_MAP: dict[str, str] = {
    "010_jungle": "Sierra 117",
    "020_base": "Crow's Nest",
    "030_outskirts": "Tsavo Highway",
    "030_docks": "Tsavo Highway",
    "040_voi": "The Storm",
    "050_floodvoi": "Floodgate",
    "070_waste": "The Ark",
    "060_ark": "The Ark",
    "100_citadel": "The Covenant",
    "070_halo": "The Covenant",
    "110_hc": "Cortana",
    "080_cortana": "Cortana",
    "120_halo": "Halo",
}
SCENARIO_TO_MISSION = SCENARIO_MAP

# Tier-based weapon license definitions
TIER_WEAPON_MAP: dict[str, list[str]] = {
    "Sidearms & Light Automatics": [
        "magnum",
        "plasma_pistol",
        "spiker",
        "smg",
        "plasma_rifle",
        "needler",
        "mauler",
    ],
    "Precision Rifles": [
        "battle_rifle",
        "covenant_carbine",
        "sentinel_beam",
    ],
    "Close-Quarters & Assault": [
        "assault_rifle",
        "shotgun",
        "brute_shot",
        "energy_sword",
        "gravity_hammer",
    ],
    "Power Weapons": [
        "sniper_rifle",
        "beam_rifle",
        "rocket_launcher",
        "spartan_laser",
        "missile_pod",
        "flak_cannon",
    ],
}

# IPC & Network Settings
DEFAULT_PIPE_NAME = r"\\.\pipe\Halo3Archipelago"
DEFAULT_TCP_HOST = "127.0.0.1"
DEFAULT_TCP_PORT = 29500
STATE_FILE_NAME = "halo3_ap_state.json"

# ============================================================================
# MEMORY ADDRESSES & OFFSETS
# If MCC or Halo 3 receives a patch/update, adjust offsets here and in
# mcc_mod/offsets.h to restore compatibility.
# ============================================================================

# MCC-Win64-Shipping.exe Offsets
MCC_SCENARIO_STRUCT_OFFSET: int = 0x03F8E778  # Pointer to active scenario loading struct
SCENARIO_STRING_OFFSET: int = 0x30            # String offset in scenario struct ("levels\solo\...")
PGCR_BASE_OFFSET: int = 0x4000B8C             # Base address for menustate
PGCR_INDICATOR_OFFSET: int = 0xC              # Offset from PGCR_BASE indicating Post-Game Carnage Report

# Hook offsets for MCC-Win64-Shipping.exe (Detoured by mcc_mod)
MCC_START_COUNTDOWN_OFFSET: int = 0x5137EC    # CampaignLobby::StartCountdown hook
MCC_ENGINE_DISPATCH_OFFSET: int = 0x42D6F8    # Engine Dispatch hook on main simulation thread

# Hook & Data Offsets for halo3.dll (Detoured by mcc_mod)
HALO3_WEAPON_CAN_INTERACT_OFFSET: int = 0x8D258   # weapon_can_interact hook
HALO3_UNIT_ADD_WEAPON_OFFSET: int = 0x358024      # unit_add_weapon hook
HALO3_ENGINE_STEP_OFFSET: int = 0xB2538           # Halo3EngineStep simulation tick hook
HALO3_TLS_INDEX_OFFSET: int = 0xA39F9C            # Thread-local storage index pointer
HALO3_TAG_INSTANCES_OFFSET: int = 0xA49018        # Tag instances table pointer
HALO3_TAG_CACHE_BASE_OFFSET: int = 0x1FCF4C8      # Tag cache base address pointer
HALO3_UNIT_DROP_WEAPON_OFFSET: int = 0x359FBC     # UnitDropWeapon function pointer
HALO3_PLACEMENT_NEW_OFFSET: int = 0x341508        # PlacementNew function pointer
HALO3_OBJECT_NEW_OFFSET: int = 0x341AE8           # ObjectNew function pointer
HALO3_UNIT_INVENTORY_OFFSET: int = 0x268          # Player unit inventory array offset
HALO3_TAG_STRING_ID_S11C_OFFSET: int = 0x11C      # Switch-To StringID offset in tag definition

# Event detection (rally points, terminals, skulls)
HALO3_INSERTION_POINT_UNLOCK_OFFSET: int = 0x1F30A0 # game_insertion_point_unlock hook
HALO3_TERMINAL_ACTIVATE_OFFSET: int = 0x3A8798      # terminal_activate hook (native device interaction)
HALO3_TERMINAL_WAS_ACCESSED_OFFSET: int = HALO3_TERMINAL_ACTIVATE_OFFSET  # alias
HALO3_AWARD_PRIMARY_SKULL_OFFSET: int = 0x136088    # campaign_metagame_award_primary_skull hook
HALO3_AWARD_SECONDARY_SKULL_OFFSET: int = 0x136580  # campaign_metagame_award_secondary_skull hook

# Mission completion & auto-return-to-menu
HALO3_GAME_WON_OFFSET: int = 0x1F2F7C               # game_won hook (mission beaten)
HALO3_GAME_SAVE_AND_QUIT_OFFSET: int = 0x1065F4     # game_save_and_quit (clean exit to menu)
