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
            "starting_mission": self.options.starting_mission.value,
            "weapon_license_mode": self.options.weapon_license_mode.value,
            "starting_weapon": self.options.starting_weapon.value,
            "include_skulls": bool(self.options.include_skulls.value),
            "include_terminals": bool(self.options.include_terminals.value),
            "include_rally_points": bool(self.options.include_rally_points.value),
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

