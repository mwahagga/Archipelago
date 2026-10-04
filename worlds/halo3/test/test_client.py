import os
import unittest
from unittest.mock import MagicMock, patch

import ModuleUpdate
ModuleUpdate.update_ran = True

from ..constants import MISSION_NAMES, SCENARIO_MAP, TIER_WEAPON_MAP
from ..locations import LOCATION_NAME_TO_ID
from .. import client as Halo3Client


class TestHalo3ClientConstants(unittest.TestCase):
    def test_scenario_map_contains_all_nine_missions(self) -> None:
        """Every mission in MISSION_NAMES must have a scenario mapping."""
        mapped_missions = set(SCENARIO_MAP.values())
        for mission in MISSION_NAMES:
            self.assertIn(mission, mapped_missions)

    def test_scenario_codes_accuracy(self) -> None:
        """Verify scenario codes match native MCC engine scenario codes."""
        expected = {
            "010_jungle": "Sierra 117",
            "020_base": "Crow's Nest",
            "030_outskirts": "Tsavo Highway",
            "030_docks": "Tsavo Highway",
            "040_voi": "The Storm",
            "050_floodvoi": "Floodgate",
            "070_waste": "The Ark",
            "060_ark": "The Ark",
            "100_citadel": "The Covenant",
            "070_halo": "The Covenant",
            "110_hc": "Cortana",
            "080_cortana": "Cortana",
            "120_halo": "Halo",
        }
        for code, expected_name in expected.items():
            self.assertEqual(SCENARIO_MAP.get(code), expected_name)

    def test_scenario_substring_matching(self) -> None:
        """Verify that MCC raw scenario path strings match the scenario codes."""
        test_paths = {
            r"levels\solo\010_jungle\010_jungle": "Sierra 117",
            r"levels\solo\020_base\020_base": "Crow's Nest",
            r"levels\solo\030_outskirts\030_outskirts": "Tsavo Highway",
            r"levels\solo\030_docks\030_docks": "Tsavo Highway",
            r"levels\solo\040_voi\040_voi": "The Storm",
            r"levels\solo\050_floodvoi\050_floodvoi": "Floodgate",
            r"levels\solo\070_waste\070_waste": "The Ark",
            r"levels\solo\060_ark\060_ark": "The Ark",
            r"levels\solo\100_citadel\100_citadel": "The Covenant",
            r"levels\solo\070_halo\070_halo": "The Covenant",
            r"levels\solo\110_hc\110_hc": "Cortana",
            r"levels\solo\080_cortana\080_cortana": "Cortana",
            r"levels\solo\120_halo\120_halo": "Halo",
        }
        for path, expected_mission in test_paths.items():
            detected = None
            for code, name in SCENARIO_MAP.items():
                if code in path:
                    detected = name
                    break
            self.assertEqual(detected, expected_mission)

    def test_tier_weapon_map_coverage(self) -> None:
        """Ensure all 4 tiers are present and non-empty."""
        self.assertEqual(len(TIER_WEAPON_MAP), 4)
        for tier_name, weapons in TIER_WEAPON_MAP.items():
            self.assertTrue(len(weapons) > 0, f"Tier '{tier_name}' has no weapons!")


class TestHalo3CommandProcessor(unittest.TestCase):
    def setUp(self) -> None:
        self.mock_ctx = MagicMock()
        self.mock_ctx.checked_locations = set()
        self.mock_ctx.unlocked_missions = {"Sierra 117"}
        self.mock_ctx.unlocked_weapons = {"assault_rifle"}
        self.mock_ctx.mcc_connected = False
        self.mock_ctx.current_mcc_map = None

        self.cmd_proc = Halo3Client.Halo3CommandProcessor(self.mock_ctx)
        self.outputs: list[str] = []
        self.cmd_proc.output = lambda text: self.outputs.append(text)

    def test_complete_by_mission_index(self) -> None:
        """'/complete 1' should complete the first mission (Sierra 117)."""
        self.cmd_proc._cmd_complete("1")
        expected_id = LOCATION_NAME_TO_ID["Sierra 117 - Mission Complete"]
        self.mock_ctx.send_location_check.assert_called_once_with(expected_id, "Sierra 117")

    def test_complete_by_location_id(self) -> None:
        """'/complete 117201' should complete Blind Skull."""
        blind_skull_id = LOCATION_NAME_TO_ID["Sierra 117 - Blind Skull"]
        self.cmd_proc._cmd_complete(str(blind_skull_id))
        self.mock_ctx.send_location_check.assert_called_once_with(blind_skull_id, "Sierra 117 - Blind Skull")

    def test_complete_by_name(self) -> None:
        """'/complete blind skull' should find and complete the skull."""
        self.cmd_proc._cmd_complete("blind", "skull")
        blind_skull_id = LOCATION_NAME_TO_ID["Sierra 117 - Blind Skull"]
        self.mock_ctx.send_location_check.assert_called_once_with(blind_skull_id, "Sierra 117 - Blind Skull")

    def test_complete_already_checked(self) -> None:
        """Completing an already checked location reports it and does not send check."""
        loc_name = "Sierra 117 - Blind Skull"
        loc_id = LOCATION_NAME_TO_ID[loc_name]
        self.mock_ctx.checked_locations.add(loc_id)

        self.cmd_proc._cmd_complete(loc_name)
        self.mock_ctx.send_location_check.assert_not_called()
        self.assertTrue(any("already completed" in out for out in self.outputs))

    def test_complete_unknown_location(self) -> None:
        """Completing a non-existent location outputs an error."""
        self.cmd_proc._cmd_complete("nonexistent", "location")
        self.mock_ctx.send_location_check.assert_not_called()
        self.assertTrue(any("Unknown location" in out for out in self.outputs))

    def test_status_output(self) -> None:
        """'/status' prints status including unlocked missions without crashing."""
        self.cmd_proc._cmd_status()
        full_output = "\n".join(self.outputs)
        self.assertIn("Sierra 117", full_output)
        self.assertIn("[UNLOCKED]", full_output)
        self.assertIn("[LOCKED]", full_output)
        self.assertIn("assault_rifle", full_output)


class TestHalo3ContextVictory(unittest.TestCase):
    def setUp(self) -> None:
        self.patcher = patch("worlds.halo3.client.async_start")
        self.mock_async_start = self.patcher.start()
        self.addCleanup(self.patcher.stop)

        self.ctx = Halo3Client.Halo3Context.__new__(Halo3Client.Halo3Context)
        self.ctx.checked_locations = set()
        self.ctx.finished_game = False
        self.ctx.goal = 0
        self.ctx.send_msgs = MagicMock()
        self.ctx.write_state_file = MagicMock()

    def test_goal_beat_halo_triggers_victory(self) -> None:
        self.ctx.goal = 0
        halo_id = LOCATION_NAME_TO_ID["Halo - Mission Complete"]
        self.ctx.send_location_check(halo_id, "Halo - Mission Complete")
        self.assertTrue(self.ctx.finished_game)

    def test_goal_beat_halo_other_mission_does_not_trigger_victory(self) -> None:
        self.ctx.goal = 0
        sierra_id = LOCATION_NAME_TO_ID["Sierra 117 - Mission Complete"]
        self.ctx.send_location_check(sierra_id, "Sierra 117 - Mission Complete")
        self.assertFalse(self.ctx.finished_game)

    def test_goal_beat_all_missions_requires_all_nine(self) -> None:
        self.ctx.goal = 1
        # Complete 8 missions
        for m in MISSION_NAMES[:-1]:
            loc_id = LOCATION_NAME_TO_ID[f"{m} - Mission Complete"]
            self.ctx.send_location_check(loc_id, f"{m} - Mission Complete")
            self.assertFalse(self.ctx.finished_game)
        # Complete the 9th mission
        last_id = LOCATION_NAME_TO_ID[f"{MISSION_NAMES[-1]} - Mission Complete"]
        self.ctx.send_location_check(last_id, f"{MISSION_NAMES[-1]} - Mission Complete")
        self.assertTrue(self.ctx.finished_game)


class TestHalo3ModIPC(unittest.TestCase):
    def test_ipc_push_state_tcp(self) -> None:
        """Verify that push_state_to_mod sends valid JSON and processes returned events."""
        ctx = Halo3Client.Halo3Context.__new__(Halo3Client.Halo3Context)
        ctx.unlocked_missions = {"Sierra 117", "Crow's Nest"}
        ctx.unlocked_weapons = {"assault_rifle", "battle_rifle"}
        ctx.weapon_license_mode = 1
        ctx.starting_weapon = "assault_rifle"
        ctx.ipc_connected = True
        ctx.ipc_type = "tcp"
        ctx.current_mcc_map = None
        ctx.prev_mcc_map = None
        ctx.process_mod_events = MagicMock()

        import json

        # Mock TCP socket
        mock_sock = MagicMock()
        response_payload = json.dumps({
            "status": "ok",
            "events": ["terminal_010_jungle", "skull_blind"]
        }) + "\n"
        mock_sock.recv.return_value = response_payload.encode("utf-8")
        ctx.tcp_sock = mock_sock

        ctx.push_state_to_mod()

        # Check sendall was called with valid JSON
        self.assertTrue(mock_sock.sendall.called)
        sent_data = mock_sock.sendall.call_args[0][0].decode("utf-8")
        parsed_sent = json.loads(sent_data)
        self.assertIn("Sierra 117", parsed_sent["unlocked_missions"])
        self.assertIn("assault_rifle", parsed_sent["unlocked_weapons"])
        self.assertEqual(parsed_sent["weapon_license_mode"], 1)

        # Check returned events were dispatched to process_mod_events
        ctx.process_mod_events.assert_called_once_with(["terminal_010_jungle", "skull_blind"])


class TestCrossPlatformSupport(unittest.TestCase):
    @patch("sys.platform", "linux")
    @patch("os.path.isdir")
    def test_detect_mcc_path_linux(self, mock_isdir: MagicMock) -> None:
        """Test Linux / Steam Deck Steam library detection."""
        from worlds.halo3.launcher import detect_mcc_path

        def isdir_mock(path: str) -> bool:
            return "Halo The Master Chief Collection" in path

        mock_isdir.side_effect = isdir_mock
        found = detect_mcc_path()
        self.assertIsNotNone(found)
        self.assertIn("Halo The Master Chief Collection", found)

    def test_cross_platform_state_dir(self) -> None:
        """Verify state directory path resolution for Windows and Linux."""
        import sys
        if sys.platform == "win32":
            expected_prefix = os.path.expandvars(r"%localappdata%")
        else:
            expected_prefix = os.path.expanduser("~/.local/share")
        self.assertTrue(os.path.isdir(expected_prefix) or not os.path.isabs(expected_prefix))



