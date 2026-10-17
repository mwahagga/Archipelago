from dataclasses import dataclass
from typing import Any
from Options import Choice, DeathLink, DeathLinkMixin, OptionGroup, PerGameCommonOptions, Range, Toggle


class Goal(Choice):
    """
    Choose the victory condition for Halo 3.
    - Beat Halo: Unlock and successfully finish the final mission 'Halo'.
    - Beat All Missions: Complete all 9 campaign missions.
    - Fast Victory: Complete a set number of unique campaign missions (configured by missions_required).
    """
    display_name = "Goal"
    option_beat_halo = 0
    option_beat_all_missions = 1
    option_fast_victory = 2
    default = option_beat_halo


class MissionsRequired(Range):
    """
    Number of unique campaign missions required to complete the game when Goal is set to Fast Victory.
    """
    display_name = "Missions Required"
    range_start = 1
    range_end = 9
    default = 5


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


class ShieldProgression(Choice):
    """
    Controls Master Chief's MJOLNIR shield capacity and upgrade progression.
    - Vanilla: Start with full 100% shields. No shield items in the pool.
    - Progressive: Start at 25% shields. Find 3 upgrades to reach 100% (+25% per upgrade).
    - Progressive + Overshield: Start at 25%. Reach 100% at Upgrade 3, then unlock Half Overshield (150%), Full Overshield (200%), Super Overshield (250%), and Triple Overshield (300%) with Upgrades 4-7.
    - Glass Cannon: Start at 0% shields (health only). Find 4 upgrades to reach 100% (+25% per upgrade).
    - Glass Cannon + Overshield: Start at 0% shields. Reach 100% at Upgrade 4, then unlock Half Overshield (150%), Full Overshield (200%), Super Overshield (250%), and Triple Overshield (300%) with Upgrades 5-8.
    """
    display_name = "Shield Progression"
    option_vanilla = 0
    option_progressive = 1
    option_progressive_overshield = 2
    option_glass_cannon = 3
    option_glass_cannon_overshield = 4
    default = option_vanilla


class DeathLinkAmnesty(Range):
    """
    Number of local deaths to forgive before broadcasting a DeathLink to the multiworld.
    0 means every death sends a DeathLink immediately.
    1 means every second death sends a DeathLink, etc.
    """
    display_name = "Death Link Amnesty"
    range_start = 0
    range_end = 10
    default = 0


class DeathLinkReceiveHits(Range):
    """
    Number of DeathLinks that must be received from other players before triggering the in-game penalty.
    1 means every received DeathLink immediately triggers the penalty.
    2 means you absorb 1 free DeathLink before suffering the penalty on the 2nd, etc.
    """
    display_name = "Death Link Receive Hits"
    range_start = 1
    range_end = 5
    default = 1


class DeathLinkEffect(Choice):
    """
    What happens to Master Chief when a DeathLink is received and triggers:
    - Kill: Instant death (checkpoint restart). True hardcore DeathLink.
    - Shield Break: Depletes MJOLNIR shields to 0% (EMP shock), leaving Chief vulnerable to 1-shot lethal damage.
    - Mortal: Depletes shields to 0% and reduces health to 10% (one hit from death).
    """
    display_name = "Death Link Effect"
    option_kill = 0
    option_shield_break = 1
    option_mortal = 2
    default = option_kill


@dataclass
class Halo3Options(PerGameCommonOptions, DeathLinkMixin):
    goal: Goal
    missions_required: MissionsRequired
    starting_mission: StartingMission
    weapon_license_mode: WeaponLicenseMode
    starting_weapon: StartingWeapon
    shield_progression: ShieldProgression
    include_skulls: IncludeSkulls
    include_terminals: IncludeTerminals
    include_rally_points: IncludeRallyPoints
    death_link_amnesty: DeathLinkAmnesty
    death_link_receive_hits: DeathLinkReceiveHits
    death_link_effect: DeathLinkEffect


option_groups = [
    OptionGroup(
        "Mission Options",
        [Goal, MissionsRequired, StartingMission],
    ),
    OptionGroup(
        "Weapon Options",
        [WeaponLicenseMode, StartingWeapon],
    ),
    OptionGroup(
        "Player Upgrades",
        [ShieldProgression],
    ),
    OptionGroup(
        "Location Checks",
        [IncludeSkulls, IncludeTerminals, IncludeRallyPoints],
    ),
    OptionGroup(
        "DeathLink Options",
        [DeathLink, DeathLinkAmnesty, DeathLinkReceiveHits, DeathLinkEffect],
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
        "goal": Goal.option_fast_victory,
        "missions_required": 4,
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

