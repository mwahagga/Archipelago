from .bases import Halo3TestBase
from ..constants import MISSION_NAMES


class TestGoalBeatHalo(Halo3TestBase):
    options = {
        "goal": 0,  # Beat Halo
        "starting_mission": 0,  # Sierra 117
    }

    def test_cannot_win_at_start(self) -> None:
        """Cannot win with only the starting mission unlocked."""
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

    def test_cannot_win_with_other_missions_alone(self) -> None:
        """Unlocking other missions without Halo does not grant victory."""
        for mission in MISSION_NAMES[:-1]:
            self.collect_by_name(f"{mission} Unlock")
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

    def test_win_with_halo_unlock(self) -> None:
        """Collecting Halo Unlock grants access to Halo 3 Victory Check."""
        self.collect_by_name("Halo Unlock")
        self.assertTrue(self.can_reach_location("Halo 3 Victory Check"))


class TestGoalBeatAllMissions(Halo3TestBase):
    options = {
        "goal": 1,  # Beat All Missions
        "starting_mission": 0,  # Sierra 117
    }

    def test_cannot_win_at_start(self) -> None:
        """Cannot win at start when Beat All Missions is required."""
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

    def test_cannot_win_with_halo_alone(self) -> None:
        """Collecting Halo Unlock alone is insufficient when Beat All Missions is chosen."""
        self.collect_by_name("Halo Unlock")
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

    def test_cannot_win_with_8_of_9_missions(self) -> None:
        """Missing even one mission unlock prevents victory."""
        for mission in MISSION_NAMES[:-1]:  # Leave out Halo
            self.collect_by_name(f"{mission} Unlock")
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

    def test_win_requires_all_9_missions(self) -> None:
        """Victory requires every single one of the 9 mission unlocks."""
        for mission in MISSION_NAMES:
            self.collect_by_name(f"{mission} Unlock")
        self.assertTrue(self.can_reach_location("Halo 3 Victory Check"))


class TestGoalFastVictory(Halo3TestBase):
    options = {
        "goal": 2,  # Fast Victory
        "missions_required": 4,
        "starting_mission": 0,  # Sierra 117
    }

    def test_cannot_win_at_start(self) -> None:
        """Cannot win with only the starting mission unlocked (1/4)."""
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

    def test_cannot_win_with_less_than_required(self) -> None:
        """Collecting 3 missions is insufficient when 4 are required."""
        self.collect_by_name("Crow's Nest Unlock")
        self.collect_by_name("Tsavo Highway Unlock")
        # Player has Sierra 117 (starter) + Crow's Nest + Tsavo Highway = 3
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

    def test_win_with_exact_required_missions(self) -> None:
        """Collecting the 4th distinct mission unlock satisfies victory."""
        self.collect_by_name("Crow's Nest Unlock")
        self.collect_by_name("Tsavo Highway Unlock")
        self.collect_by_name("The Storm Unlock")
        # 4 unique missions
        self.assertTrue(self.can_reach_location("Halo 3 Victory Check"))

    def test_win_with_any_combination_of_missions(self) -> None:
        """Any 4 unique mission unlocks grant victory, regardless of which ones."""
        self.collect_by_name("Floodgate Unlock")
        self.collect_by_name("The Ark Unlock")
        self.collect_by_name("The Covenant Unlock")
        self.assertTrue(self.can_reach_location("Halo 3 Victory Check"))


class TestVictoryShieldRequirements(Halo3TestBase):
    options = {
        "goal": 0,  # Beat Halo
        "shield_progression": 1,  # Progressive
        "starting_mission": 0,  # Sierra 117
    }

    def test_halo_victory_requires_shields(self) -> None:
        """Beating Halo with shield progression requires both Halo Unlock AND 3 Shield Upgrades."""
        self.collect_by_name("Halo Unlock")
        # With 0 shields, cannot enter Halo Region
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

        # Add 1 shield (1/3, insufficient)
        self.collect(self.get_item_by_name("Progressive Shield Upgrade"))
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

        # Add 2nd shield (2/3, insufficient: Halo requires Tier 3 = 3 upgrades)
        self.collect(self.get_item_by_name("Progressive Shield Upgrade"))
        self.assertFalse(self.can_reach_location("Halo 3 Victory Check"))

        # Add 3rd shield -> Halo is now accessible -> victory check reachable!
        self.collect(self.get_item_by_name("Progressive Shield Upgrade"))
        self.assertTrue(self.can_reach_location("Halo 3 Victory Check"))

