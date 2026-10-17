from collections.abc import Mapping
from typing import Any, ClassVar
import settings
from worlds.AutoWorld import World
from . import items, locations, regions, rules, web_world
from . import options as halo3_options


class Halo3Settings(settings.Group):
    class MccInstallDirectory(settings.OptionalUserFolderPath):
        """
        Path to the Halo: The Master Chief Collection install directory.
        If omitted or empty, automatic detection via Steam registry/libraries will be used.
        """

    mcc_install_directory: MccInstallDirectory = MccInstallDirectory("")


class Halo3World(World):
    """
    Halo 3 Archipelago Randomizer.
    Master Chief's campaign missions unlocked and randomized through the multiworld.
    """
    game = "Halo 3"
    web = web_world.Halo3WebWorld()

    settings_key = "halo3_settings"
    settings: ClassVar[Halo3Settings]

    options_dataclass = halo3_options.Halo3Options
    options: halo3_options.Halo3Options

    location_name_to_id = locations.LOCATION_NAME_TO_ID
    item_name_to_id = items.ITEM_NAME_TO_ID

    origin_region_name = "Campaign Menu"

    def generate_early(self) -> None:
        from Options import OptionError

        total_locs = 9
        if self.options.include_skulls:
            total_locs += 9
        if self.options.include_terminals:
            total_locs += 7
        if self.options.include_rally_points:
            total_locs += 18

        mandatory = 8  # 8 non-starter mission unlocks
        if self.options.weapon_license_mode.value == 1:
            mandatory += 17
        elif self.options.weapon_license_mode.value == 2:
            mandatory += 3

        shield_counts = {1: 3, 2: 7, 3: 4, 4: 8}
        needed_shields = shield_counts.get(self.options.shield_progression.value, 0)
        if mandatory + needed_shields > total_locs:
            raise OptionError(
                f"Cannot generate Halo 3 seed for player '{self.player_name}': requested options require "
                f"{mandatory + needed_shields} items (8 mission unlocks, {needed_shields} shield upgrades, "
                f"{mandatory - 8} weapon licenses), but only {total_locs} locations are enabled. "
                f"Please enable skulls, terminals, or rally points, or adjust shield/license options."
            )

        if self.options.shield_progression.value in (1, 2, 3, 4):
            # Prioritize shield upgrades into sphere 1 (starting mission checks)
            # 1 upgrade for progressive modes (3-7 total), 2 for glass cannon modes (4-8 total).
            num_early = 2 if self.options.shield_progression.value in (3, 4) else 1
            existing = self.multiworld.local_early_items[self.player].get("Progressive Shield Upgrade", 0)
            self.multiworld.local_early_items[self.player]["Progressive Shield Upgrade"] = max(existing, num_early)

    def create_regions(self) -> None:
        regions.create_and_connect_regions(self)
        locations.create_all_locations(self)

    def set_rules(self) -> None:
        rules.set_all_rules(self)

    def create_items(self) -> None:
        items.create_all_items(self)

    def create_item(self, name: str) -> items.Halo3Item:
        return items.create_item_with_correct_classification(self, name)

    def get_filler_item_name(self) -> str:
        return items.get_random_filler_item_name(self)

    def fill_slot_data(self) -> Mapping[str, Any]:
        return {
            "goal": self.options.goal.value,
            "missions_required": self.options.missions_required.value,
            "starting_mission": self.options.starting_mission.value,
            "weapon_license_mode": self.options.weapon_license_mode.value,
            "starting_weapon": self.options.starting_weapon.value,
            "shield_progression": self.options.shield_progression.value,
            "include_skulls": bool(self.options.include_skulls.value),
            "include_terminals": bool(self.options.include_terminals.value),
            "include_rally_points": bool(self.options.include_rally_points.value),
            "death_link": bool(self.options.death_link.value),
            "death_link_amnesty": self.options.death_link_amnesty.value,
            "death_link_receive_hits": self.options.death_link_receive_hits.value,
            "death_link_effect": self.options.death_link_effect.value,
        }

    def generate_output(self, output_directory: str) -> None:
        import os
        from .container import Halo3Container

        out_file_base = self.multiworld.get_out_file_name_base(self.player)
        container_path = os.path.join(output_directory, f"{out_file_base}{Halo3Container.patch_file_ending}")
        container = Halo3Container(
            path=container_path,
            player=self.player,
            player_name=self.player_name,
            slot_data=dict(self.fill_slot_data()),
        )
        container.write()

