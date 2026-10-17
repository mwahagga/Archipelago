from __future__ import annotations
from typing import TYPE_CHECKING
from worlds.generic.Rules import set_rule
from .constants import MISSION_NAMES

if TYPE_CHECKING:
    from .world import Halo3World


# Mission shield requirements when shield progression is active:
# - Tier 0 (0 upgrades / base capacity): Sierra 117 (introductory jungle level)
# - Tier 1 (1 upgrade / 50% capacity): Crow's Nest (early interior combat)
# - Tier 2 (2 upgrades / 75% capacity): Tsavo Highway, The Storm, Floodgate, The Ark (vehicle combat, heavy swarms)
# - Tier 3 (3 upgrades): The Covenant, Cortana, Halo (scarabs, brute chieftains, pure-form swarms)
#   Note: In Progressive mode (25% base), 3 upgrades = 100% capacity. In Glass Cannon mode (0% base),
#   3 upgrades grants access at 75% capacity, intentionally avoiding over-constraining seed generation
#   by requiring all 4 base upgrades.
MISSION_SHIELD_REQUIREMENTS: dict[str, int] = {
    "Sierra 117": 0,
    "Crow's Nest": 1,
    "Tsavo Highway": 2,
    "The Storm": 2,
    "Floodgate": 2,
    "The Ark": 2,
    "The Covenant": 3,
    "Cortana": 3,
    "Halo": 3,
}


def set_all_rules(world: Halo3World) -> None:
    shield_mode = world.options.shield_progression.value
    starting_choice = world.options.starting_mission.value
    starting_mission = MISSION_NAMES[starting_choice] if 0 <= starting_choice < len(MISSION_NAMES) else MISSION_NAMES[0]

    # Set entrance rules for each mission
    for mission in MISSION_NAMES:
        entrance = world.get_entrance(f"Start {mission}")
        unlock_item = f"{mission} Unlock"
        req_shields = 0
        if shield_mode != 0 and mission != starting_mission:
            req_shields = MISSION_SHIELD_REQUIREMENTS.get(mission, 0)

        if req_shields > 0:
            set_rule(
                entrance,
                lambda state, item=unlock_item, shields=req_shields: (
                    state.has(item, world.player)
                    and state.has("Progressive Shield Upgrade", world.player, shields)
                ),
            )
        else:
            set_rule(entrance, lambda state, item=unlock_item: state.has(item, world.player))

    # Set victory rule
    victory_check = world.get_location("Halo 3 Victory Check")
    if world.options.goal.value == 0:  # Beat Halo
        set_rule(victory_check, lambda state: state.can_reach_location("Halo - Mission Complete", world.player))
    elif world.options.goal.value == 1:  # Beat All Missions
        all_mission_locs = [f"{m} - Mission Complete" for m in MISSION_NAMES]
        set_rule(victory_check, lambda state: all(state.can_reach_location(loc, world.player) for loc in all_mission_locs))
    elif world.options.goal.value == 2:  # Fast Victory (Any X Unique Missions)
        count = world.options.missions_required.value
        all_mission_locs = [f"{m} - Mission Complete" for m in MISSION_NAMES]
        set_rule(
            victory_check,
            lambda state, c=count, locs=all_mission_locs: sum(1 for loc in locs if state.can_reach_location(loc, world.player)) >= c,
        )

    world.multiworld.completion_condition[world.player] = lambda state: state.has("Victory", world.player)
