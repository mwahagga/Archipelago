from __future__ import annotations
from typing import TYPE_CHECKING
from BaseClasses import Entrance, Region
from .constants import MISSION_NAMES

if TYPE_CHECKING:
    from .world import Halo3World


def create_and_connect_regions(world: Halo3World) -> None:
    menu = Region("Campaign Menu", world.player, world.multiworld)
    world.multiworld.regions.append(menu)

    for mission in MISSION_NAMES:
        region = Region(mission, world.player, world.multiworld)
        world.multiworld.regions.append(region)

        # Connect Menu -> Mission via Entrance
        entrance_name = f"Start {mission}"
        entrance = Entrance(world.player, entrance_name, parent=menu)
        menu.exits.append(entrance)
        entrance.connect(region)
