from .bases import Halo3TestBase
from ..constants import MISSION_NAMES


class TestHalo3Basic(Halo3TestBase):
    def test_default_world_generation(self) -> None:
        """Verify that Halo 3 generates successfully with default options."""
        self.assertIsNotNone(self.world)
        self.assertEqual(self.world.game, "Halo 3")

    def test_default_starting_mission_precollected(self) -> None:
        """Verify that default starting mission (Sierra 117) is precollected."""
        self.assertIn("Sierra 117 Unlock", [item.name for item in self.multiworld.precollected_items[self.player]])

    def test_default_starting_weapon_precollected(self) -> None:
        """Verify that default starting weapons (Assault Rifle and Plasma Pistol) are precollected."""
        precollected = [item.name for item in self.multiworld.precollected_items[self.player]]
        self.assertIn("Assault Rifle License", precollected)
        self.assertIn("Plasma Pistol License", precollected)

    def test_location_and_item_counts_match(self) -> None:
        """Verify that itempool count matches unfilled locations count."""
        locations_count = len(self.multiworld.get_unfilled_locations(self.player))
        # Note: 1 victory location is pre-filled with Victory event item
        # Multiworld itempool should match the number of unfilled locations
        self.assertEqual(len(self.multiworld.itempool), locations_count)

    def test_all_missions_exist_in_regions(self) -> None:
        """Verify all canonical missions exist as regions."""
        region_names = {region.name for region in self.multiworld.regions if region.player == self.player}
        for mission in MISSION_NAMES:
            self.assertIn(mission, region_names)
        self.assertIn("Campaign Menu", region_names)


class TestHalo3Presets(Halo3TestBase):
    def test_presets_exist_and_valid(self) -> None:
        """Verify options_presets are defined and contain expected presets."""
        from ..options import options_presets
        from ..web_world import Halo3WebWorld

        self.assertEqual(Halo3WebWorld.options_presets, options_presets)
        self.assertIn("Standard", options_presets)
        self.assertIn("Quick Campaign", options_presets)
        self.assertIn("Full Campaign (All Missions)", options_presets)
        self.assertIn("Tiered Weapons", options_presets)
        self.assertIn("Vanilla Combat", options_presets)

