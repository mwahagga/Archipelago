from __future__ import annotations
from typing import TYPE_CHECKING
from BaseClasses import Item, ItemClassification

if TYPE_CHECKING:
    from .world import Halo3World

from .constants import MISSION_NAMES

# Base item ID: 117000
ITEM_NAME_TO_ID: dict[str, int] = {
    # Mission Unlocks (117001 - 117009)
    "Sierra 117 Unlock": 117001,
    "Crow's Nest Unlock": 117002,
    "Tsavo Highway Unlock": 117003,
    "The Storm Unlock": 117004,
    "Floodgate Unlock": 117005,
    "The Ark Unlock": 117006,
    "The Covenant Unlock": 117007,
    "Cortana Unlock": 117008,
    "Halo Unlock": 117009,

    # Tier-based Weapon Licenses (117011 - 117014)
    "Sidearms & Light Automatics License": 117011,
    "Precision Rifles License": 117012,
    "Close-Quarters & Assault License": 117013,
    "Power Weapons License": 117014,

    # Individual Weapon Licenses (117020 - 117038)
    "Assault Rifle License": 117020,
    "Magnum License": 117021,
    "Plasma Pistol License": 117022,
    "Spiker License": 117023,
    "Battle Rifle License": 117024,
    "Covenant Carbine License": 117025,
    "SMG License": 117026,
    "Plasma Rifle License": 117027,
    "Needler License": 117028,
    "Mauler License": 117029,
    "Shotgun License": 117030,
    "Brute Shot License": 117031,
    "Sniper Rifle License": 117032,
    "Beam Rifle License": 117033,
    "Rocket Launcher License": 117034,
    "Spartan Laser License": 117035,
    "Missile Pod License": 117036,
    "Energy Sword License": 117037,
    "Gravity Hammer License": 117038,

    # Player Upgrades (117040)
    "Progressive Shield Upgrade": 117040,

    # Filler Items (117050)
    "Moa Burger": 117050,
}

INDIVIDUAL_WEAPON_LICENSES = [
    "Assault Rifle License",
    "Magnum License",
    "Plasma Pistol License",
    "Spiker License",
    "Battle Rifle License",
    "Covenant Carbine License",
    "SMG License",
    "Plasma Rifle License",
    "Needler License",
    "Mauler License",
    "Shotgun License",
    "Brute Shot License",
    "Sniper Rifle License",
    "Beam Rifle License",
    "Rocket Launcher License",
    "Spartan Laser License",
    "Missile Pod License",
    "Energy Sword License",
    "Gravity Hammer License",
]

STARTER_WEAPON_LICENSES = [
    "Assault Rifle License",
    "Magnum License",
    "Plasma Pistol License",
    "Spiker License",
]

TIER_WEAPON_LICENSES = [
    "Sidearms & Light Automatics License",
    "Precision Rifles License",
    "Close-Quarters & Assault License",
    "Power Weapons License",
]

DEFAULT_ITEM_CLASSIFICATIONS: dict[str, ItemClassification] = {
    # Mission Unlocks
    "Sierra 117 Unlock": ItemClassification.progression,
    "Crow's Nest Unlock": ItemClassification.progression,
    "Tsavo Highway Unlock": ItemClassification.progression,
    "The Storm Unlock": ItemClassification.progression,
    "Floodgate Unlock": ItemClassification.progression,
    "The Ark Unlock": ItemClassification.progression,
    "The Covenant Unlock": ItemClassification.progression,
    "Cortana Unlock": ItemClassification.progression,
    "Halo Unlock": ItemClassification.progression,

    # Tier Weapon Licenses
    "Sidearms & Light Automatics License": ItemClassification.progression,
    "Precision Rifles License": ItemClassification.progression,
    "Close-Quarters & Assault License": ItemClassification.progression,
    "Power Weapons License": ItemClassification.progression,

    # Individual Weapon Licenses
    "Assault Rifle License": ItemClassification.progression,
    "Magnum License": ItemClassification.progression,
    "Plasma Pistol License": ItemClassification.progression,
    "Spiker License": ItemClassification.progression,
    "Battle Rifle License": ItemClassification.progression,
    "Covenant Carbine License": ItemClassification.progression,
    "SMG License": ItemClassification.progression,
    "Plasma Rifle License": ItemClassification.progression,
    "Needler License": ItemClassification.progression,
    "Mauler License": ItemClassification.progression,
    "Shotgun License": ItemClassification.progression,
    "Brute Shot License": ItemClassification.progression,
    "Sniper Rifle License": ItemClassification.progression,
    "Beam Rifle License": ItemClassification.progression,
    "Rocket Launcher License": ItemClassification.progression,
    "Spartan Laser License": ItemClassification.progression,
    "Missile Pod License": ItemClassification.progression,
    "Energy Sword License": ItemClassification.progression,
    "Gravity Hammer License": ItemClassification.progression,

    # Player Upgrades
    "Progressive Shield Upgrade": ItemClassification.progression,

    # Fillers
    "Moa Burger": ItemClassification.filler,
}

FILLER_ITEMS = [
    "Moa Burger",
]


class Halo3Item(Item):
    game = "Halo 3"


def get_random_filler_item_name(world: Halo3World) -> str:
    return world.random.choice(FILLER_ITEMS)


def create_item_with_correct_classification(world: Halo3World, name: str) -> Halo3Item:
    classification = DEFAULT_ITEM_CLASSIFICATIONS.get(name, ItemClassification.filler)
    return Halo3Item(name, classification, ITEM_NAME_TO_ID[name], world.player)


def create_all_items(world: Halo3World) -> None:
    # 1. Determine starting mission unlock
    starting_choice = world.options.starting_mission.value
    starting_mission = MISSION_NAMES[starting_choice]
    starting_unlock = f"{starting_mission} Unlock"
    world.push_precollected(world.create_item(starting_unlock))

    itempool: list[Item] = []

    # Add the remaining mission unlocks to the itempool
    for mission in MISSION_NAMES:
        unlock_name = f"{mission} Unlock"
        if unlock_name != starting_unlock:
            itempool.append(world.create_item(unlock_name))

    # 2. Weapon Licenses according to weapon_license_mode
    license_mode = world.options.weapon_license_mode.value
    if license_mode == 1:  # Individual
        # Determine starting weapon(s):
        # We always guarantee both a kinetic weapon and an energy sidearm (Plasma Pistol)
        # to ensure the player has a viable anti-shield combat loop and never suffers early-game ammo starvation.
        starting_weap_opt = world.options.starting_weapon.value
        if starting_weap_opt == 4:  # Random Starter
            starter_license = world.random.choice(STARTER_WEAPON_LICENSES)
        else:
            starter_license = STARTER_WEAPON_LICENSES[starting_weap_opt]

        starter_licenses: set[str] = {starter_license, "Plasma Pistol License"}
        if starter_license == "Plasma Pistol License":
            starter_licenses.add("Assault Rifle License")

        # Give starting licenses to player
        for license_name in sorted(starter_licenses):
            world.push_precollected(world.create_item(license_name))

        # Add remaining weapon licenses to item pool
        for license_name in INDIVIDUAL_WEAPON_LICENSES:
            if license_name not in starter_licenses:
                itempool.append(world.create_item(license_name))

    elif license_mode == 2:  # Tiers
        # Starter tier
        starter_tier = "Sidearms & Light Automatics License"
        world.push_precollected(world.create_item(starter_tier))

        for tier_name in TIER_WEAPON_LICENSES:
            if tier_name != starter_tier:
                itempool.append(world.create_item(tier_name))

    # 3. Shield Upgrades according to shield_progression
    shield_mode = world.options.shield_progression.value
    shield_counts = {
        1: 3,  # progressive: 25% base + 3 upgrades -> 100%
        2: 7,  # progressive_overshield: 25% base + 7 upgrades -> 300% (Triple Overshield)
        3: 4,  # glass_cannon: 0% base + 4 upgrades -> 100%
        4: 8,  # glass_cannon_overshield: 0% base + 8 upgrades -> 300% (Triple Overshield)
    }
    num_shields = shield_counts.get(shield_mode, 0)
    unfilled = len(world.multiworld.get_unfilled_locations(world.player))
    remaining_space = max(0, unfilled - len(itempool))
    actual_shields = min(num_shields, remaining_space)
    for _ in range(actual_shields):
        itempool.append(world.create_item("Progressive Shield Upgrade"))

    # 4. Fill any remaining unfilled locations with filler equipment items
    needed_filler = unfilled - len(itempool)
    for _ in range(max(0, needed_filler)):
        itempool.append(world.create_filler())

    world.multiworld.itempool += itempool
