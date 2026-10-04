from dataclasses import dataclass
from typing import Any
from Options import Choice, OptionGroup, PerGameCommonOptions, Toggle


class Goal(Choice):
    """
    Choose the victory condition for Halo 3.
    - Beat Halo: Unlock and successfully finish the final mission 'Halo'.
    - Beat All Missions: Complete all 9 campaign missions.
    """
    display_name = "Goal"
    option_beat_halo = 0
    option_beat_all_missions = 1
    default = option_beat_halo


class StartingMission(Choice):
    """
    Which mission starts unlocked.
    """
    display_name = "Starting Mission"
    option_sierra_117 = 0
    option_crows_nest = 1
    option_tsavo_highway = 2
    option_the_storm = 3
    option_floodgate = 4
    option_the_ark = 5
    option_the_covenant = 6
    option_cortana = 7
    option_halo = 8
    default = option_sierra_117


class WeaponLicenseMode(Choice):
    """
    Controls whether weapons require Archipelago license items to use and pick up.
    - Disabled: Vanilla weapon access. All weapons can be picked up freely.
    - Individual: Every weapon has its own specific license item (19 items).
    - Tiers: Weapons are bundled into 4 broad weapon class licenses.
    """
    display_name = "Weapon License Mode"
    option_disabled = 0
    option_individual = 1
    option_tiers = 2
    default = option_individual


class StartingWeapon(Choice):
    """
    Which primary weapon license you begin the game with when individual weapon licenses are enabled.
    In addition, the Plasma Pistol License is always guaranteed as a starting energy sidearm
    to prevent early-game ammo starvation against shielded enemies.
    """
    display_name = "Starting Weapon"
    option_assault_rifle = 0
    option_magnum = 1
    option_plasma_pistol = 2
    option_spiker = 3
    option_random_starter = 4
    default = option_assault_rifle


class IncludeSkulls(Toggle):
    """
    Add the 13 campaign skulls as Archipelago location checks.
    Picking up a skull sends an immediate check.
    """
    display_name = "Include Skulls"
    default = True


class IncludeTerminals(Toggle):
    """
    Add the 7 Forerunner terminals (on The Ark, The Covenant, and Halo) as location checks.
    Accessing a terminal sends a check.
    """
    display_name = "Include Terminals"
    default = True


class IncludeRallyPoints(Toggle):
    """
    Add Rally Points Bravo and Charlie across campaign missions as location checks (18 checks).
    Reaching the checkpoint milestone sends a check.
    """
    display_name = "Include Rally Points"
    default = True


@dataclass
class Halo3Options(PerGameCommonOptions):
    goal: Goal
    starting_mission: StartingMission
    weapon_license_mode: WeaponLicenseMode
    starting_weapon: StartingWeapon
    include_skulls: IncludeSkulls
    include_terminals: IncludeTerminals
    include_rally_points: IncludeRallyPoints


option_groups = [
    OptionGroup(
        "Mission Options",
        [Goal, StartingMission],
    ),
    OptionGroup(
        "Weapon Options",
        [WeaponLicenseMode, StartingWeapon],
    ),
    OptionGroup(
        "Location Checks",
        [IncludeSkulls, IncludeTerminals, IncludeRallyPoints],
    ),
]


options_presets: dict[str, dict[str, Any]] = {
    "Standard": {
        "goal": Goal.option_beat_halo,
        "starting_mission": StartingMission.option_sierra_117,
        "weapon_license_mode": WeaponLicenseMode.option_individual,
        "starting_weapon": StartingWeapon.option_assault_rifle,
        "include_skulls": True,
        "include_terminals": True,
        "include_rally_points": True,
    },
    "Quick Campaign": {
        "goal": Goal.option_beat_halo,
        "starting_mission": StartingMission.option_sierra_117,
        "weapon_license_mode": WeaponLicenseMode.option_disabled,
        "include_skulls": False,
        "include_terminals": False,
        "include_rally_points": False,
    },
    "Full Campaign (All Missions)": {
        "goal": Goal.option_beat_all_missions,
        "starting_mission": StartingMission.option_sierra_117,
        "weapon_license_mode": WeaponLicenseMode.option_individual,
        "starting_weapon": StartingWeapon.option_random_starter,
        "include_skulls": True,
        "include_terminals": True,
        "include_rally_points": True,
    },
    "Tiered Weapons": {
        "goal": Goal.option_beat_halo,
        "starting_mission": StartingMission.option_sierra_117,
        "weapon_license_mode": WeaponLicenseMode.option_tiers,
        "include_skulls": True,
        "include_terminals": True,
        "include_rally_points": True,
    },
    "Vanilla Combat": {
        "goal": Goal.option_beat_halo,
        "starting_mission": StartingMission.option_sierra_117,
        "weapon_license_mode": WeaponLicenseMode.option_disabled,
        "include_skulls": True,
        "include_terminals": True,
        "include_rally_points": True,
    },
}

