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
