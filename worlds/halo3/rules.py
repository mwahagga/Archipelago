from __future__ import annotations
from typing import TYPE_CHECKING
from worlds.generic.Rules import set_rule
from .constants import MISSION_NAMES

if TYPE_CHECKING:
    from .world import Halo3World


def set_all_rules(world: Halo3World) -> None:
    # Set entrance rules for each mission
    for mission in MISSION_NAMES:
        entrance = world.get_entrance(f"Start {mission}")
        unlock_item = f"{mission} Unlock"
        set_rule(entrance, lambda state, item=unlock_item: state.has(item, world.player))

    # Set victory rule
    victory_check = world.get_location("Halo 3 Victory Check")
    if world.options.goal.value == 0:  # Beat Halo
        set_rule(victory_check, lambda state: state.has("Halo Unlock", world.player))
    else:  # Beat All Missions
        all_unlocks = [f"{m} Unlock" for m in MISSION_NAMES]
        set_rule(victory_check, lambda state: state.has_all(all_unlocks, world.player))

    world.multiworld.completion_condition[world.player] = lambda state: state.has("Victory", world.player)
