from .bases import Halo3TestBase
from ..constants import MISSION_NAMES


class TestDefaultMissions(Halo3TestBase):
    options = {
        "starting_mission": 0,  # Sierra 117
    }

    def test_starting_mission_accessible_without_items(self) -> None:
        """Sierra 117 should be accessible from the start with empty state."""
        self.assertTrue(self.can_reach_region("Sierra 117"))

    def test_locked_missions_inaccessible_without_unlocks(self) -> None:
        """Missions other than the starting mission must not be accessible initially."""
        for mission in MISSION_NAMES[1:]:
            self.assertFalse(
                self.can_reach_region(mission),
                f"Region '{mission}' was reachable without its unlock item!"
            )

    def test_collecting_unlock_grants_access(self) -> None:
        """Collecting a mission's unlock item should grant access to that mission's region."""
        for mission in MISSION_NAMES[1:]:
            unlock_name = f"{mission} Unlock"
            self.assertFalse(self.can_reach_region(mission))
            self.collect_by_name(unlock_name)
            self.assertTrue(
                self.can_reach_region(mission),
                f"Region '{mission}' was still unreachable after collecting '{unlock_name}'!"
            )


class TestDifferentStartingMission(Halo3TestBase):
    options = {
        "starting_mission": 5,  # The Ark
    }

    def test_the_ark_starter_precollected(self) -> None:
        """Choosing The Ark as starting mission precollects The Ark Unlock."""
        precollected = [item.name for item in self.multiworld.precollected_items[self.player]]
        self.assertIn("The Ark Unlock", precollected)
        self.assertNotIn("Sierra 117 Unlock", precollected)

    def test_the_ark_accessible_from_start(self) -> None:
        """The Ark should be reachable from start, but Sierra 117 should be locked."""
        self.assertTrue(self.can_reach_region("The Ark"))
        self.assertFalse(self.can_reach_region("Sierra 117"))


class TestLocationOptions(Halo3TestBase):
    options = {
        "include_skulls": 1,
        "include_terminals": 1,
        "include_rally_points": 1,
    }

    def test_all_locations_present_when_enabled(self) -> None:
        """9 completions + 13 skulls + 7 terminals + 18 rally points = 47 check locations (+1 victory event)."""
        locations = list(self.world.get_locations())
        # 47 locations + 1 event location
        self.assertEqual(len(locations), 48)

        # Check skull exists
        self.assertIsNotNone(self.world.get_location("Sierra 117 - Blind Skull"))
        # Check terminal exists
        self.assertIsNotNone(self.world.get_location("The Ark - Terminal 1"))
        # Check rally point exists
        self.assertIsNotNone(self.world.get_location("Sierra 117 - Rally Point Bravo"))


class TestMinimalLocations(Halo3TestBase):
    options = {
        "include_skulls": 0,
        "include_terminals": 0,
        "include_rally_points": 0,
        "weapon_license_mode": 0,  # Vanilla weapons so 8 mission unlocks fit in 9 locations
    }

    def test_only_mission_completions_generated(self) -> None:
        """With skulls, terminals, and rally points disabled, only 9 completions (+ 1 victory event) exist."""
        locations = list(self.world.get_locations())
        self.assertEqual(len(locations), 10)  # 9 + 1 victory event
        for mission in MISSION_NAMES:
            self.assertIsNotNone(self.world.get_location(f"{mission} - Mission Complete"))
