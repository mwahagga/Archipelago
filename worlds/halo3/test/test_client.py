import asyncio
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

    def test_goal_fast_victory_requires_threshold(self) -> None:
        self.ctx.goal = 2  # Fast Victory
        self.ctx.missions_required = 3
        # Complete 2 unique missions
        self.ctx.send_location_check(LOCATION_NAME_TO_ID["Sierra 117 - Mission Complete"], "Sierra 117 - Mission Complete")
        self.ctx.send_location_check(LOCATION_NAME_TO_ID["Crow's Nest - Mission Complete"], "Crow's Nest - Mission Complete")
        self.assertFalse(self.ctx.finished_game)

        # Complete the 3rd unique mission
        self.ctx.send_location_check(LOCATION_NAME_TO_ID["Tsavo Highway - Mission Complete"], "Tsavo Highway - Mission Complete")
        self.assertTrue(self.ctx.finished_game)

    def test_goal_fast_victory_requires_unique_missions(self) -> None:
        self.ctx.goal = 2  # Fast Victory
        self.ctx.missions_required = 3
        # Completing the same mission multiple times does not increment count
        sierra_id = LOCATION_NAME_TO_ID["Sierra 117 - Mission Complete"]
        self.ctx.send_location_check(sierra_id, "Sierra 117 - Mission Complete")
        self.ctx.send_location_check(sierra_id, "Sierra 117 - Mission Complete")
        self.assertFalse(self.ctx.finished_game)
        self.assertEqual(len(self.ctx.checked_locations), 1)


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

    def test_deathlink_receive_hits_and_penalty(self) -> None:
        """Verify DeathLink receive hits accumulation and penalty dispatch."""
        ctx = Halo3Client.Halo3Context.__new__(Halo3Client.Halo3Context)
        ctx.death_link = True
        ctx.death_link_receive_hits = 2
        ctx.death_link_effect = 1  # Shield Break
        ctx.received_deaths_count = 0
        ctx.death_link_cooldown = 0.0
        ctx.pending_mod_deathlink = None
        ctx.is_penalty_death_pending = False
        ctx.current_mcc_map = "Sierra 117"
        ctx.write_state_file = MagicMock()

        # Hit 1: absorbed, no penalty
        ctx.on_deathlink({"time": 100.0, "source": "PlayerB", "cause": "PlayerB died"})
        self.assertEqual(ctx.received_deaths_count, 1)
        self.assertIsNone(ctx.pending_mod_deathlink)

        # Hit 2: triggers shield_break penalty
        ctx.on_deathlink({"time": 101.0, "source": "PlayerB", "cause": "PlayerB died again"})
        self.assertEqual(ctx.received_deaths_count, 0)
        self.assertEqual(ctx.pending_mod_deathlink, "shield_break")
        self.assertTrue(ctx.write_state_file.called)

    def test_deathlink_menu_receive_and_penalty_suppression(self) -> None:
        """Verify incoming DeathLinks work in menu, queue penalty, and suppress outward broadcast on death."""
        ctx = Halo3Client.Halo3Context.__new__(Halo3Client.Halo3Context)
        ctx.death_link = True
        ctx.death_link_receive_hits = 1
        ctx.death_link_effect = 0  # Kill
        ctx.received_deaths_count = 0
        ctx.death_link_cooldown = 0.0
        ctx.pending_mod_deathlink = None
        ctx.is_penalty_death_pending = False
        ctx.current_mcc_map = None  # In menu!
        ctx.player_names = {1: "Master Chief"}
        ctx.slot = 1
        ctx.write_state_file = MagicMock()
        ctx.dispatch_async = MagicMock()

        # 1. Incoming DeathLink received while in menu queues penalty
        ctx.on_deathlink({"time": 100.0, "source": "PlayerB", "cause": "PlayerB died"})
        self.assertEqual(ctx.pending_mod_deathlink, "kill")
        self.assertTrue(ctx.is_penalty_death_pending)

        # 2. Player enters mission and dies from the penalty
        ctx.current_mcc_map = "Sierra 117"
        ctx._handle_player_death_event()

        # Outward broadcast must NOT have been called!
        self.assertFalse(ctx.dispatch_async.called)
        self.assertFalse(ctx.is_penalty_death_pending)

    def test_deathlink_send_amnesty(self) -> None:
        """Verify natural local death amnesty absorbs deaths before broadcasting."""
        ctx = Halo3Client.Halo3Context.__new__(Halo3Client.Halo3Context)
        ctx.death_link = True
        ctx.death_link_amnesty = 2
        ctx.local_deaths_count = 0
        ctx.death_link_cooldown = 0.0
        ctx.is_penalty_death_pending = False
        ctx.player_names = {1: "Master Chief"}
        ctx.slot = 1
        ctx.current_mcc_map = "Sierra 117"
        def dummy_dispatch(coro):
            if asyncio.iscoroutine(coro):
                coro.close()
        ctx.dispatch_async = MagicMock(side_effect=dummy_dispatch)

        # Death 1: absorbed by amnesty
        ctx._handle_player_death_event()
        self.assertEqual(ctx.local_deaths_count, 1)
        self.assertFalse(ctx.dispatch_async.called)

        # Death 2: absorbed by amnesty
        ctx._handle_player_death_event()
        self.assertEqual(ctx.local_deaths_count, 2)
        self.assertFalse(ctx.dispatch_async.called)

        # Death 3: broadcasts death
        ctx._handle_player_death_event()
        self.assertEqual(ctx.local_deaths_count, 0)
        self.assertTrue(ctx.dispatch_async.called)

    def test_deathlink_command_toggle_and_test(self) -> None:
        """Verify /deathlink command toggles status and test runs in menu."""
        ctx = Halo3Client.Halo3Context.__new__(Halo3Client.Halo3Context)
        ctx.death_link = False
        ctx.death_link_amnesty = 1
        ctx.death_link_receive_hits = 1
        ctx.death_link_effect = 0
        ctx.received_deaths_count = 0
        ctx.death_link_cooldown = 0.0
        ctx.current_mcc_map = None  # In menu!
        ctx.pending_mod_deathlink = None
        ctx.is_penalty_death_pending = False
        ctx.write_state_file = MagicMock()

        def dummy_dispatch(coro):
            if asyncio.iscoroutine(coro):
                coro.close()
        ctx.dispatch_async = MagicMock(side_effect=dummy_dispatch)

        proc = Halo3Client.Halo3CommandProcessor(ctx)
        proc.output = MagicMock()

        proc._cmd_deathlink("on")
        self.assertTrue(ctx.death_link)
        self.assertTrue(ctx.dispatch_async.called)

        proc._cmd_deathlink("off")
        self.assertFalse(ctx.death_link)

        # /deathlink test works even while in menu!
        proc._cmd_deathlink("test", "kill")
        self.assertEqual(ctx.pending_mod_deathlink, "kill")
        self.assertTrue(ctx.is_penalty_death_pending)


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
        self.assertTrue(os.path.isdir(expected_prefix) or not os.path.isabs(expected_prefix))


class TestHalo3ShieldCalculations(unittest.TestCase):
    def test_shield_fractions_all_modes(self) -> None:
        """Verify get_max_shield_fraction calculations across all 5 shield progression modes."""
        ctx = Halo3Client.Halo3Context.__new__(Halo3Client.Halo3Context)

        # 0: Vanilla (always 1.0)
        ctx.shield_progression = 0
        for count in range(10):
            ctx.shield_upgrade_count = count
            self.assertEqual(ctx.get_max_shield_fraction(), 1.0)

        # 1: Progressive (25%, 50%, 75%, 100%)
        ctx.shield_progression = 1
        expected_progressive = [0.25, 0.50, 0.75, 1.00, 1.00]
        for count, expected in enumerate(expected_progressive):
            ctx.shield_upgrade_count = count
            self.assertAlmostEqual(ctx.get_max_shield_fraction(), expected)

        # 2: Progressive + Overshield (25%, 50%, 75%, 100%, 150%, 200%, 250%, 300%)
        ctx.shield_progression = 2
        expected_prog_over = [0.25, 0.50, 0.75, 1.00, 1.50, 2.00, 2.50, 3.00, 3.00]
        for count, expected in enumerate(expected_prog_over):
            ctx.shield_upgrade_count = count
            self.assertAlmostEqual(ctx.get_max_shield_fraction(), expected)

        # 3: Glass Cannon (0%, 25%, 50%, 75%, 100%)
        ctx.shield_progression = 3
        expected_glass = [0.00, 0.25, 0.50, 0.75, 1.00, 1.00]
        for count, expected in enumerate(expected_glass):
            ctx.shield_upgrade_count = count
            self.assertAlmostEqual(ctx.get_max_shield_fraction(), expected)

        # 4: Glass Cannon + Overshield (0%, 25%, 50%, 75%, 100%, 150%, 200%, 250%, 300%)
        ctx.shield_progression = 4
        expected_glass_over = [0.00, 0.25, 0.50, 0.75, 1.00, 1.50, 2.00, 2.50, 3.00, 3.00]
        for count, expected in enumerate(expected_glass_over):
            ctx.shield_upgrade_count = count
            self.assertAlmostEqual(ctx.get_max_shield_fraction(), expected)



