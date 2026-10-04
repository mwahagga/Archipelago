from __future__ import annotations
from typing import TYPE_CHECKING
from BaseClasses import Location
from . import items
from .constants import MISSION_NAMES

if TYPE_CHECKING:
    from .world import Halo3World

# Mission Completions (117101 - 117109)
MISSION_LOCATION_TO_REGION: dict[str, str] = {
    f"{mission} - Mission Complete": mission for mission in MISSION_NAMES
}

# Campaign Skulls (117201 - 117213)
SKULL_LOCATION_TO_REGION: dict[str, str] = {
    "Sierra 117 - Blind Skull": "Sierra 117",
    "Sierra 117 - Iron Skull": "Sierra 117",
    "Crow's Nest - Black Eye Skull": "Crow's Nest",
    "Crow's Nest - Grunt Birthday Party Skull": "Crow's Nest",
    "Tsavo Highway - Tough Luck Skull": "Tsavo Highway",
    "The Storm - Catch Skull": "The Storm",
    "Floodgate - Fog Skull": "Floodgate",
    "The Ark - Famine Skull": "The Ark",
    "The Ark - Cowbell Skull": "The Ark",
    "The Covenant - Thunderstorm Skull": "The Covenant",
    "Cortana - Tilt Skull": "Cortana",
    "The Covenant - IWHBYD Skull": "The Covenant",
    "Halo - Mythic Skull": "Halo",
}

# Forerunner Terminals (117301 - 117307)
TERMINAL_LOCATION_TO_REGION: dict[str, str] = {
    "The Ark - Terminal 1": "The Ark",
    "The Ark - Terminal 2": "The Ark",
    "The Ark - Terminal 3": "The Ark",
    "The Covenant - Terminal 1": "The Covenant",
    "The Covenant - Terminal 2": "The Covenant",
    "The Covenant - Terminal 3": "The Covenant",
    "Halo - Terminal 1": "Halo",
}

# Mission Rally Points (117401 - 117418)
RALLY_POINT_LOCATION_TO_REGION: dict[str, str] = {
    "Sierra 117 - Rally Point Bravo": "Sierra 117",
    "Sierra 117 - Rally Point Charlie": "Sierra 117",
    "Crow's Nest - Rally Point Bravo": "Crow's Nest",
    "Crow's Nest - Rally Point Charlie": "Crow's Nest",
    "Tsavo Highway - Rally Point Bravo": "Tsavo Highway",
    "Tsavo Highway - Rally Point Charlie": "Tsavo Highway",
    "The Storm - Rally Point Bravo": "The Storm",
    "The Storm - Rally Point Charlie": "The Storm",
    "Floodgate - Rally Point Bravo": "Floodgate",
    "Floodgate - Rally Point Charlie": "Floodgate",
    "The Ark - Rally Point Bravo": "The Ark",
    "The Ark - Rally Point Charlie": "The Ark",
    "The Covenant - Rally Point Bravo": "The Covenant",
    "The Covenant - Rally Point Charlie": "The Covenant",
    "Cortana - Rally Point Bravo": "Cortana",
    "Cortana - Rally Point Charlie": "Cortana",
    "Halo - Rally Point Bravo": "Halo",
    "Halo - Rally Point Charlie": "Halo",
}

LOCATION_NAME_TO_ID: dict[str, int] = {
    # Mission Completions
    "Sierra 117 - Mission Complete": 117101,
    "Crow's Nest - Mission Complete": 117102,
    "Tsavo Highway - Mission Complete": 117103,
    "The Storm - Mission Complete": 117104,
    "Floodgate - Mission Complete": 117105,
    "The Ark - Mission Complete": 117106,
    "The Covenant - Mission Complete": 117107,
    "Cortana - Mission Complete": 117108,
    "Halo - Mission Complete": 117109,

    # Campaign Skulls
    "Sierra 117 - Blind Skull": 117201,
    "Sierra 117 - Iron Skull": 117202,
    "Crow's Nest - Black Eye Skull": 117203,
    "Crow's Nest - Grunt Birthday Party Skull": 117204,
    "Tsavo Highway - Tough Luck Skull": 117205,
    "The Storm - Catch Skull": 117206,
    "Floodgate - Fog Skull": 117207,
    "The Ark - Famine Skull": 117208,
    "The Ark - Cowbell Skull": 117209,
    "The Covenant - Thunderstorm Skull": 117210,
    "Cortana - Tilt Skull": 117211,
    "The Covenant - IWHBYD Skull": 117212,
    "Halo - Mythic Skull": 117213,

    # Forerunner Terminals
    "The Ark - Terminal 1": 117301,
    "The Ark - Terminal 2": 117302,
    "The Ark - Terminal 3": 117303,
    "The Covenant - Terminal 1": 117304,
    "The Covenant - Terminal 2": 117305,
    "The Covenant - Terminal 3": 117306,
    "Halo - Terminal 1": 117307,

    # Rally Points
    "Sierra 117 - Rally Point Bravo": 117401,
    "Sierra 117 - Rally Point Charlie": 117402,
    "Crow's Nest - Rally Point Bravo": 117403,
    "Crow's Nest - Rally Point Charlie": 117404,
    "Tsavo Highway - Rally Point Bravo": 117405,
    "Tsavo Highway - Rally Point Charlie": 117406,
    "The Storm - Rally Point Bravo": 117407,
    "The Storm - Rally Point Charlie": 117408,
    "Floodgate - Rally Point Bravo": 117409,
    "Floodgate - Rally Point Charlie": 117410,
    "The Ark - Rally Point Bravo": 117411,
    "The Ark - Rally Point Charlie": 117412,
    "The Covenant - Rally Point Bravo": 117413,
    "The Covenant - Rally Point Charlie": 117414,
    "Cortana - Rally Point Bravo": 117415,
    "Cortana - Rally Point Charlie": 117416,
    "Halo - Rally Point Bravo": 117417,
    "Halo - Rally Point Charlie": 117418,
}

MISSION_TO_LOCATION: dict[str, str] = {
    mission: f"{mission} - Mission Complete" for mission in MISSION_NAMES
}


class Halo3Location(Location):
    game = "Halo 3"


def create_all_locations(world: Halo3World) -> None:
    # 1. Mission Completion locations (always active)
    for loc_name, region_name in MISSION_LOCATION_TO_REGION.items():
        region = world.get_region(region_name)
        location = Halo3Location(
            world.player,
            loc_name,
            LOCATION_NAME_TO_ID[loc_name],
            region,
        )
        region.locations.append(location)

    # 2. Campaign Skull locations
    if world.options.include_skulls.value:
        for loc_name, region_name in SKULL_LOCATION_TO_REGION.items():
            region = world.get_region(region_name)
            location = Halo3Location(
                world.player,
                loc_name,
                LOCATION_NAME_TO_ID[loc_name],
                region,
            )
            region.locations.append(location)

    # 3. Forerunner Terminal locations
    if world.options.include_terminals.value:
        for loc_name, region_name in TERMINAL_LOCATION_TO_REGION.items():
            region = world.get_region(region_name)
            location = Halo3Location(
                world.player,
                loc_name,
                LOCATION_NAME_TO_ID[loc_name],
                region,
            )
            region.locations.append(location)

    # 4. Mission Rally Point locations
    if world.options.include_rally_points.value:
        for loc_name, region_name in RALLY_POINT_LOCATION_TO_REGION.items():
            region = world.get_region(region_name)
            location = Halo3Location(
                world.player,
                loc_name,
                LOCATION_NAME_TO_ID[loc_name],
                region,
            )
            region.locations.append(location)

    # Victory event in the victory-checking region
    goal_region_name = "Halo" if world.options.goal.value == 0 else "Campaign Menu"
    goal_region = world.get_region(goal_region_name)
    goal_region.add_event(
        "Halo 3 Victory Check",
        "Victory",
        location_type=Halo3Location,
        item_type=items.Halo3Item,
    )
