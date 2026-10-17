import os
import shutil
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from worlds.halo3 import client as Halo3Client
from worlds.halo3 import launcher
import subprocess
from worlds.halo3.launcher import (
    detect_mcc_path,
    get_bundled_dxgi_path,
    get_mcc_dxgi_path,
    sanitize_mcc_directory,
    deploy_mod,
    cleanup_mod,
    launch_mcc,
    spawn_watchdog,
    MCC_EXE_REL_PATH,
    DXGI_REL_PATH,
)


class TestLauncherPaths(unittest.TestCase):
    def test_bundled_dxgi_exists(self) -> None:
        """The bundled dxgi.dll in worlds/halo3/mod/ must exist on disk."""
        bundled = get_bundled_dxgi_path()
        self.assertTrue(os.path.isfile(bundled), f"Bundled mod DLL not found at: {bundled}")

    def test_get_mcc_dxgi_path(self) -> None:
        """Destination path must correctly point to mcc/binaries/win64/dxgi.dll."""
        base = r"C:\Games\Halo MCC"
        dest = get_mcc_dxgi_path(base)
        self.assertEqual(dest, os.path.join(base, DXGI_REL_PATH))

    @patch("settings.get_settings")
    def test_detect_mcc_path_from_settings(self, mock_get_settings: MagicMock) -> None:
        """detect_mcc_path should prioritize Archipelago settings if configured."""
        with tempfile.TemporaryDirectory() as tmpdir:
            mcc_exe = os.path.join(tmpdir, MCC_EXE_REL_PATH)
            os.makedirs(os.path.dirname(mcc_exe), exist_ok=True)
            with open(mcc_exe, "w") as f:
                f.write("mock_exe")

            mock_settings = MagicMock()
            mock_settings.halo3_settings.mcc_install_directory = tmpdir
            mock_get_settings.return_value = mock_settings

            detected = detect_mcc_path()
            self.assertEqual(detected, os.path.abspath(tmpdir))


class TestModDeploymentAndCleanup(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.win64_dir = os.path.join(self.temp_dir, "mcc", "binaries", "win64")
        os.makedirs(self.win64_dir, exist_ok=True)
        self.mcc_exe = os.path.join(self.win64_dir, "MCC-Win64-Shipping.exe")
        with open(self.mcc_exe, "w") as f:
            f.write("mock")

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_deploy_and_cleanup_mod(self) -> None:
        """deploy_mod places dxgi.dll; cleanup_mod removes it."""
        dxgi_dest = os.path.join(self.win64_dir, "dxgi.dll")
        self.assertFalse(os.path.exists(dxgi_dest))

        # 1. Deploy
        success = deploy_mod(self.temp_dir)
        self.assertTrue(success)
        self.assertTrue(os.path.isfile(dxgi_dest))

        # 2. Cleanup
        success_clean = cleanup_mod(self.temp_dir)
        self.assertTrue(success_clean)
        self.assertFalse(os.path.exists(dxgi_dest))

    @patch("worlds.halo3.launcher.is_mcc_running", return_value=False)
    def test_sanitize_mcc_directory_when_not_running(self, mock_running: MagicMock) -> None:
        """Stale dxgi.dll is swept away when MCC is not running."""
        dxgi_dest = os.path.join(self.win64_dir, "dxgi.dll")
        with open(dxgi_dest, "w") as f:
            f.write("stale_dll")
        self.assertTrue(os.path.exists(dxgi_dest))

        cleaned = sanitize_mcc_directory(self.temp_dir)
        self.assertTrue(cleaned)
        self.assertFalse(os.path.exists(dxgi_dest))

    @patch("worlds.halo3.launcher.is_mcc_running", return_value=True)
    def test_sanitize_skips_when_mcc_running(self, mock_running: MagicMock) -> None:
        """Sanitization does not touch files if MCC is actively running."""
        dxgi_dest = os.path.join(self.win64_dir, "dxgi.dll")
        with open(dxgi_dest, "w") as f:
            f.write("active_dll")

        cleaned = sanitize_mcc_directory(self.temp_dir)
        self.assertFalse(cleaned)
        self.assertTrue(os.path.exists(dxgi_dest))


class TestLaunchWorkflow(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.win64_dir = os.path.join(self.temp_dir, "mcc", "binaries", "win64")
        os.makedirs(self.win64_dir, exist_ok=True)
        self.mcc_exe = os.path.join(self.win64_dir, "MCC-Win64-Shipping.exe")
        with open(self.mcc_exe, "w") as f:
            f.write("mock")

    def tearDown(self) -> None:
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    @patch("worlds.halo3.launcher.spawn_watchdog")
    @patch("subprocess.Popen")
    @patch("worlds.halo3.launcher.is_mcc_running", return_value=False)
    def test_launch_mcc_success(
        self, mock_running: MagicMock, mock_popen: MagicMock, mock_watchdog: MagicMock
    ) -> None:
        """launch_mcc deploys mod, invokes MCC with -eac-nop-loaded, and spawns watchdog."""
        mock_proc = MagicMock()
        mock_proc.pid = 12345
        mock_popen.return_value = mock_proc

        proc = launch_mcc(self.temp_dir)
        self.assertIsNotNone(proc)
        self.assertEqual(proc.pid, 12345)

        # Verify command arguments passed to subprocess.Popen
        mock_popen.assert_called_once()
        cmd_args = mock_popen.call_args[0][0]
        self.assertIn("-eac-nop-loaded", cmd_args)
        self.assertEqual(cmd_args[0], self.mcc_exe)

        # Verify watchdog spawned with process PID
        dxgi_dest = os.path.join(self.win64_dir, "dxgi.dll")
        mock_watchdog.assert_called_once_with(12345, dxgi_dest)

    @patch("worlds.halo3.launcher.spawn_watchdog")
    @patch("worlds.halo3.launcher.get_mcc_pid", return_value=54321)
    @patch("os.startfile", create=True)
    @patch("worlds.halo3.launcher.is_mcc_running", return_value=False)
    def test_launch_mcc_steam_success(
        self, mock_running: MagicMock, mock_startfile: MagicMock, mock_get_pid: MagicMock, mock_watchdog: MagicMock
    ) -> None:
        """launch_mcc via Steam URL returns LaunchedProcess with PID and spawns watchdog."""
        steam_dir = os.path.join(self.temp_dir, "steamapps", "common", "Halo The Master Chief Collection")
        win64_dir = os.path.join(steam_dir, "mcc", "binaries", "win64")
        os.makedirs(win64_dir, exist_ok=True)
        with open(os.path.join(win64_dir, "MCC-Win64-Shipping.exe"), "w") as f:
            f.write("mock")

        proc = launch_mcc(steam_dir)
        self.assertIsNotNone(proc)
        self.assertEqual(proc.pid, 54321)
        mock_watchdog.assert_called_once_with(54321, os.path.join(win64_dir, "dxgi.dll"))

    @patch("worlds.halo3.launcher.is_mcc_running", return_value=True)
    def test_launch_aborts_if_mcc_already_running(self, mock_running: MagicMock) -> None:
        """launch_mcc must not launch a second instance if MCC is already running."""
        proc = launch_mcc(self.temp_dir)
        self.assertIsNone(proc)


class TestClientLauncherCommands(unittest.TestCase):
    def setUp(self) -> None:
        self.mock_ctx = MagicMock()
        self.cmd_proc = Halo3Client.Halo3CommandProcessor(self.mock_ctx)
        self.outputs: list[str] = []
        self.cmd_proc.output = lambda text: self.outputs.append(text)

    @patch("worlds.halo3.client.is_mcc_running", return_value=False)
    @patch("worlds.halo3.client.launch_mcc")
    @patch("worlds.halo3.client.detect_mcc_path", return_value=r"C:\Mock\MCC")
    def test_cmd_launch(
        self, mock_detect: MagicMock, mock_launch: MagicMock, mock_running: MagicMock
    ) -> None:
        """'/launch' launches MCC and outputs success message."""
        mock_proc = MagicMock()
        mock_proc.pid = 9999
        mock_launch.return_value = mock_proc

        self.cmd_proc._cmd_launch()
        mock_launch.assert_called_once_with(r"C:\Mock\MCC")
        full_output = "\n".join(self.outputs)
        self.assertIn("launched successfully", full_output)
        self.assertIn("9999", full_output)

    @patch("worlds.halo3.client.is_mcc_running", return_value=False)
    @patch("worlds.halo3.client.cleanup_mod", return_value=True)
    @patch("worlds.halo3.client.detect_mcc_path", return_value=r"C:\Mock\MCC")
    def test_cmd_clean_mod(
        self, mock_detect: MagicMock, mock_clean: MagicMock, mock_running: MagicMock
    ) -> None:
        """'/clean_mod' removes dxgi.dll and reports clean status."""
        self.cmd_proc._cmd_clean_mod()
        mock_clean.assert_called_once_with(r"C:\Mock\MCC")
        full_output = "\n".join(self.outputs)
        self.assertIn("Successfully removed dxgi.dll", full_output)

    @patch("worlds.halo3.client.detect_mcc_path", return_value=r"C:\Mock\MCC")
    @patch("worlds.halo3.client.is_mcc_running", return_value=False)
    def test_cmd_mod_status(self, mock_running: MagicMock, mock_detect: MagicMock) -> None:
        """'/mod_status' outputs MCC path, mod status, and watchdog info."""
        self.cmd_proc._cmd_mod_status()
        full_output = "\n".join(self.outputs)
        self.assertIn("Halo 3 Mod & MCC Status", full_output)
        self.assertIn(r"C:\Mock\MCC", full_output)
        self.assertIn("Watchdog Supervisor", full_output)


class TestHalo3PatchContainer(unittest.TestCase):
    def test_create_and_parse_aphalo3(self) -> None:
        """Halo3Container writes .aphalo3 and parse_args parses it automatically."""
        import zipfile
        import json
        from worlds.halo3.container import Halo3Container

        with tempfile.TemporaryDirectory() as tmpdir:
            patch_path = os.path.join(tmpdir, "AP_12345_P1_Chief.aphalo3")
            container = Halo3Container(
                path=patch_path,
                player=1,
                player_name="Chief",
                server="archipelago.gg:38281",
                slot_data={"goal": 0, "starting_weapon": 0},
            )
            container.write()

            self.assertTrue(os.path.isfile(patch_path))

            # Verify zip contents
            with zipfile.ZipFile(patch_path, "r") as zf:
                self.assertIn("archipelago.json", zf.namelist())
                self.assertIn("halo3_slot_data.json", zf.namelist())
                manifest = json.loads(zf.read("archipelago.json").decode("utf-8"))
                self.assertEqual(manifest["game"], "Halo 3")
                self.assertEqual(manifest["player_name"], "Chief")
                self.assertEqual(manifest["server"], "archipelago.gg:38281")

            # Verify client parser extracts metadata from container
            parsed = Halo3Client.parse_args([patch_path])
            self.assertEqual(parsed.name, "Chief")
            self.assertEqual(parsed.connect, "archipelago.gg:38281")
            self.assertTrue(parsed.launch)


class TestSpawnWatchdog(unittest.TestCase):
    def test_spawn_watchdog_noop_for_invalid_input(self) -> None:
        """spawn_watchdog should do nothing if PID <= 0 or path is empty."""
        with patch("subprocess.Popen") as mock_popen:
            spawn_watchdog(0, r"C:\test\dxgi.dll")
            spawn_watchdog(-1, r"C:\test\dxgi.dll")
            spawn_watchdog(1234, "")
            mock_popen.assert_not_called()

    @patch("subprocess.Popen")
    def test_spawn_watchdog_windows_flags(self, mock_popen: MagicMock) -> None:
        """On Windows, spawn_watchdog configures CREATE_NO_WINDOW and SW_HIDE with DEVNULL streams."""
        with patch("sys.platform", "win32"), patch("os.path.isfile", return_value=True):
            spawn_watchdog(12345, r"C:\test\dxgi.dll")
            mock_popen.assert_called_once()
            _, kwargs = mock_popen.call_args
            self.assertEqual(kwargs.get("stdin"), subprocess.DEVNULL)
            self.assertEqual(kwargs.get("stdout"), subprocess.DEVNULL)
            self.assertEqual(kwargs.get("stderr"), subprocess.DEVNULL)
            self.assertEqual(kwargs.get("creationflags"), 0x08000000)
            startupinfo = kwargs.get("startupinfo")
            self.assertIsNotNone(startupinfo)
            self.assertEqual(startupinfo.wShowWindow, 0)

    @patch("subprocess.Popen")
    def test_spawn_watchdog_linux_flags(self, mock_popen: MagicMock) -> None:
        """On Linux, spawn_watchdog sets start_new_session=True with DEVNULL streams."""
        with patch("sys.platform", "linux"):
            spawn_watchdog(12345, "/opt/mcc/dxgi.dll")
            mock_popen.assert_called_once()
            _, kwargs = mock_popen.call_args
            self.assertEqual(kwargs.get("stdin"), subprocess.DEVNULL)
            self.assertEqual(kwargs.get("stdout"), subprocess.DEVNULL)
            self.assertEqual(kwargs.get("stderr"), subprocess.DEVNULL)
            self.assertTrue(kwargs.get("start_new_session"))
            self.assertNotIn("creationflags", kwargs)

