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


class TestShieldProgressionRules(Halo3TestBase):
    options = {
        "shield_progression": 1,  # Progressive
        "starting_mission": 0,    # Sierra 117
    }

    def test_early_items_configured(self) -> None:
        """Progressive shield mode sets 1 shield upgrade in local_early_items."""
        early_shields = self.multiworld.local_early_items[self.player].get("Progressive Shield Upgrade", 0)
        self.assertEqual(early_shields, 1)

    def test_sierra_requires_no_shields_starter(self) -> None:
        """Sierra 117 (Tier 0 starter) requires 0 shield upgrades."""
        self.assertTrue(self.can_reach_region("Sierra 117"))

    def test_crows_nest_requires_one_shield_upgrade(self) -> None:
        """Crow's Nest (Tier 1) requires 1 shield upgrade in logic."""
        shields = self.get_items_by_name("Progressive Shield Upgrade")
        self.collect_by_name("Crow's Nest Unlock")
        self.assertFalse(self.can_reach_region("Crow's Nest"))
        self.collect(shields[0])
        self.assertTrue(self.can_reach_region("Crow's Nest"))

    def test_mid_game_requires_two_shield_upgrades(self) -> None:
        """Tsavo Highway (Tier 2) requires 2 shield upgrades in logic."""
        shields = self.get_items_by_name("Progressive Shield Upgrade")
        self.collect_by_name("Tsavo Highway Unlock")
        self.assertFalse(self.can_reach_region("Tsavo Highway"))
        self.collect(shields[0])  # 1 upgrade
        self.assertFalse(self.can_reach_region("Tsavo Highway"))
        self.collect(shields[1])  # 2 upgrades
        self.assertTrue(self.can_reach_region("Tsavo Highway"))

    def test_late_game_requires_three_shield_upgrades(self) -> None:
        """The Covenant (Tier 3) requires 3 shield upgrades (100% normal shields) in logic."""
        shields = self.get_items_by_name("Progressive Shield Upgrade")
        self.collect_by_name("The Covenant Unlock")
        self.assertFalse(self.can_reach_region("The Covenant"))
        self.collect(shields[0])  # 1 upgrade
        self.assertFalse(self.can_reach_region("The Covenant"))
        self.collect(shields[1])  # 2 upgrades
        self.assertFalse(self.can_reach_region("The Covenant"))
        self.collect(shields[2])  # 3 upgrades (full normal shields)
        self.assertTrue(self.can_reach_region("The Covenant"))


class TestShieldProgressionStarterExemption(Halo3TestBase):
    options = {
        "shield_progression": 1,  # Progressive
        "starting_mission": 6,    # The Covenant
    }

    def test_starter_mission_exempt_from_shield_requirement(self) -> None:
        """The Covenant is normally Tier 3 (3 shields), but when chosen as starting mission, requires 0."""
        self.assertTrue(self.can_reach_region("The Covenant"))

    def test_other_missions_still_require_shields(self) -> None:
        """Non-starter missions like Tsavo Highway still enforce Tier 2 shield requirements (2 upgrades)."""
        shields = self.get_items_by_name("Progressive Shield Upgrade")
        self.collect_by_name("Tsavo Highway Unlock")
        self.assertFalse(self.can_reach_region("Tsavo Highway"))
        self.collect(shields[0])
        self.assertFalse(self.can_reach_region("Tsavo Highway"))
        self.collect(shields[1])
        self.assertTrue(self.can_reach_region("Tsavo Highway"))


class TestGlassCannonEarlyItems(Halo3TestBase):
    options = {
        "shield_progression": 3,  # Glass Cannon
        "starting_mission": 0,    # Sierra 117
    }

    def test_glass_cannon_prioritizes_two_early_shields(self) -> None:
        """Glass cannon mode sets 2 shield upgrades in local_early_items to recover from 0% capacity."""
        early_shields = self.multiworld.local_early_items[self.player].get("Progressive Shield Upgrade", 0)
        self.assertEqual(early_shields, 2)

