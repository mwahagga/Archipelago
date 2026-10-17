from __future__ import annotations
import logging
import os
import pkgutil
import shutil
import subprocess
import sys
import time
from typing import Optional, Any

if sys.platform == "win32":
    import ctypes
    from ctypes import wintypes
    import winreg
else:
    ctypes = None  # type: ignore
    wintypes = None  # type: ignore
    winreg = None  # type: ignore

logger = logging.getLogger("Halo3Launcher")

STEAM_APP_ID = "976730"
MCC_EXE_REL_PATH = os.path.join("mcc", "binaries", "win64", "MCC-Win64-Shipping.exe")
DXGI_REL_PATH = os.path.join("mcc", "binaries", "win64", "dxgi.dll")


def get_bundled_dxgi_path() -> str:
    """Return the absolute path to the precompiled dxgi.dll bundled in the repository."""
    # Check worlds/halo3/mod/dxgi.dll
    bundled = os.path.join(os.path.dirname(__file__), "mod", "dxgi.dll")
    if os.path.isfile(bundled):
        return os.path.abspath(bundled)

    # Fallback to mcc_mod/dxgi.dll if developing from repo root
    repo_mcc_mod = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "..", "mcc_mod", "dxgi.dll")
    )
    if os.path.isfile(repo_mcc_mod):
        return repo_mcc_mod

    return bundled


def get_mcc_dxgi_path(mcc_dir: str) -> str:
    """Return the destination path of dxgi.dll in MCC's win64 binary directory."""
    return os.path.join(mcc_dir, DXGI_REL_PATH)


def detect_mcc_path() -> Optional[str]:
    """Detect the Halo: The Master Chief Collection installation directory.

    Checks:
    1. Archipelago host.yaml / settings if configured
    2. Windows: Registry (Uninstall & Valve Steam) and libraryfolders.vdf
    3. Linux / Steam Deck: Standard and Flatpak Steam library locations
    4. Default standard Steam installation directories
    """
    # 1. Check Archipelago settings
    try:
        from settings import get_settings
        settings_path = getattr(get_settings().halo3_settings, "mcc_install_directory", None)
        if settings_path and os.path.isdir(settings_path):
            exe_check = os.path.join(settings_path, MCC_EXE_REL_PATH)
            if os.path.isfile(exe_check):
                return os.path.abspath(settings_path)
    except Exception:
        pass

    # 2. Windows registry and library detection
    if sys.platform == "win32" and winreg is not None:
        subkeys = [
            rf"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App {STEAM_APP_ID}",
            rf"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App {STEAM_APP_ID}",
        ]
        for root in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
            for subkey in subkeys:
                try:
                    with winreg.OpenKey(root, subkey) as key:
                        loc, _ = winreg.QueryValueEx(key, "InstallLocation")
                        if loc and os.path.isdir(loc):
                            exe_path = os.path.join(loc, MCC_EXE_REL_PATH)
                            if os.path.isfile(exe_path):
                                return os.path.abspath(loc)
                except OSError:
                    pass

        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as key:
                steam_path, _ = winreg.QueryValueEx(key, "SteamPath")
                if steam_path and os.path.isdir(steam_path):
                    direct = os.path.join(steam_path, "steamapps", "common", "Halo The Master Chief Collection")
                    if os.path.isfile(os.path.join(direct, MCC_EXE_REL_PATH)):
                        return os.path.abspath(direct)

                    vdf_path = os.path.join(steam_path, "steamapps", "libraryfolders.vdf")
                    if os.path.isfile(vdf_path):
                        with open(vdf_path, "r", encoding="utf-8", errors="ignore") as f:
                            for line in f:
                                if '"path"' in line.lower():
                                    parts = line.split('"')
                                    if len(parts) >= 4:
                                        lib_path = parts[3].replace("\\\\", "\\")
                                        candidate = os.path.join(lib_path, "steamapps", "common", "Halo The Master Chief Collection")
                                        if os.path.isfile(os.path.join(candidate, MCC_EXE_REL_PATH)):
                                            return os.path.abspath(candidate)
        except OSError:
            pass

    # 3. Linux / Steam Deck standard paths
    if sys.platform != "win32":
        linux_candidates = [
            os.path.expanduser("~/.local/share/Steam/steamapps/common/Halo The Master Chief Collection"),
            os.path.expanduser("~/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Halo The Master Chief Collection"),
            os.path.expanduser("~/.steam/steam/steamapps/common/Halo The Master Chief Collection"),
        ]
        for candidate in linux_candidates:
            if os.path.isfile(os.path.join(candidate, MCC_EXE_REL_PATH)):
                return os.path.abspath(candidate)

        # Check libraryfolders.vdf on Linux
        vdf_candidates = [
            os.path.expanduser("~/.local/share/Steam/steamapps/libraryfolders.vdf"),
            os.path.expanduser("~/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/libraryfolders.vdf"),
            os.path.expanduser("~/.steam/steam/steamapps/libraryfolders.vdf"),
        ]
        for vdf_path in vdf_candidates:
            if os.path.isfile(vdf_path):
                try:
                    with open(vdf_path, "r", encoding="utf-8", errors="ignore") as f:
                        for line in f:
                            if '"path"' in line.lower():
                                parts = line.split('"')
                                if len(parts) >= 4:
                                    lib_path = parts[3]
                                    candidate = os.path.join(lib_path, "steamapps", "common", "Halo The Master Chief Collection")
                                    if os.path.isfile(os.path.join(candidate, MCC_EXE_REL_PATH)):
                                        return os.path.abspath(candidate)
                except Exception:
                    pass

    # 4. Standard default installation locations (Windows fallback)
    common_roots = [
        r"C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection",
        r"C:\Program Files\Steam\steamapps\common\Halo The Master Chief Collection",
        r"D:\SteamLibrary\steamapps\common\Halo The Master Chief Collection",
        r"E:\SteamLibrary\steamapps\common\Halo The Master Chief Collection",
    ]
    for path in common_roots:
        if os.path.isfile(os.path.join(path, MCC_EXE_REL_PATH)):
            return os.path.abspath(path)

    return None


def is_mcc_running() -> bool:
    """Return True if any MCC process is currently running."""
    target_exes = {"mcc-win64-shipping.exe"}
    if sys.platform == "win32" and ctypes is not None:
        kernel32 = ctypes.windll.kernel32
        h_snap = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
        if h_snap == -1:
            return False

        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", wintypes.DWORD),
                ("cntUsage", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", wintypes.DWORD),
                ("cntThreads", wintypes.DWORD),
                ("th32ParentProcessID", wintypes.DWORD),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", wintypes.DWORD),
                ("szExeFile", ctypes.c_char * 260),
            ]

        pe = PROCESSENTRY32()
        pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
        found = False

        if kernel32.Process32First(h_snap, ctypes.byref(pe)):
            while True:
                exe_name = pe.szExeFile.decode("latin1", errors="ignore").lower()
                if exe_name in target_exes:
                    found = True
                    break
                if not kernel32.Process32Next(h_snap, ctypes.byref(pe)):
                    break

        kernel32.CloseHandle(h_snap)
        return found
    else:
        try:
            for pid_dir in os.listdir("/proc"):
                if not pid_dir.isdigit():
                    continue
                try:
                    cmdline_path = os.path.join("/proc", pid_dir, "cmdline")
                    if os.path.isfile(cmdline_path):
                        with open(cmdline_path, "rb") as f:
                            cmdline = f.read().decode("latin1", errors="ignore").lower()
                            if any(target in cmdline for target in target_exes):
                                return True
                except (OSError, IOError):
                    continue
        except Exception:
            pass
        return False


def sanitize_mcc_directory(mcc_dir: Optional[str] = None) -> bool:
    """Pre-flight check: If dxgi.dll exists in MCC and MCC is NOT running, remove it immediately.

    Ensures players never launch normal Halo with EasyAntiCheat while a stale mod DLL is present.
    """
    if not mcc_dir:
        mcc_dir = detect_mcc_path()
    if not mcc_dir:
        return False

    dxgi_dest = get_mcc_dxgi_path(mcc_dir)
    if not os.path.exists(dxgi_dest):
        return True

    if is_mcc_running():
        logger.warning(f"[Sanitizer] dxgi.dll is present at {dxgi_dest}, but MCC is currently running. Skipping deletion.")
        return False

    try:
        os.remove(dxgi_dest)
        logger.info(f"[Sanitizer] Removed stale dxgi.dll from {dxgi_dest} to protect anti-cheat integrity.")
    except Exception as e:
        logger.error(f"[Sanitizer] Failed to remove stale dxgi.dll from {dxgi_dest}: {e}")
        return False

    appid_dest = get_mcc_steam_appid_path(mcc_dir)
    if os.path.exists(appid_dest):
        try:
            os.remove(appid_dest)
        except Exception:
            pass

    return True


def get_mcc_steam_appid_path(mcc_dir: str) -> str:
    """Return the absolute path to steam_appid.txt in MCC's win64 directory."""
    return os.path.join(mcc_dir, "mcc", "binaries", "win64", "steam_appid.txt")


def deploy_mod(mcc_dir: str) -> bool:
    """Deploy dxgi.dll into MCC's win64 directory.
    Supports copying from local file system or extracting from packaged .apworld.
    """
    dest = get_mcc_dxgi_path(mcc_dir)
    dest_dir = os.path.dirname(dest)
    os.makedirs(dest_dir, exist_ok=True)

    # 1. First check if a physical file is available on disk
    src = get_bundled_dxgi_path()
    if os.path.isfile(src):
        try:
            shutil.copyfile(src, dest)
            logger.info(f"[Mod Manager] Deployed Archipelago mod DLL: {dest}")
        except Exception as e:
            logger.error(f"[Mod Manager] Failed to copy mod DLL to {dest}: {e}")
            return False
    else:
        # 2. Extract from package / .apworld zip archive using pkgutil
        try:
            dll_bytes = pkgutil.get_data("worlds.halo3", "mod/dxgi.dll")
            if not dll_bytes:
                dll_bytes = pkgutil.get_data(__name__.rsplit(".", 1)[0], "mod/dxgi.dll")
            if not dll_bytes:
                logger.error("[Mod Manager] Could not load bundled mod DLL from package or disk!")
                return False
            with open(dest, "wb") as f:
                f.write(dll_bytes)
            logger.info(f"[Mod Manager] Extracted bundled mod DLL from package to: {dest}")
        except Exception as e:
            logger.error(f"[Mod Manager] Failed to extract mod DLL to {dest}: {e}")
            return False

    # For Steam installations, write steam_appid.txt so SteamAPI_RestartAppIfNecessary doesn't restart into EAC
    if "steam" in mcc_dir.lower():
        appid_path = get_mcc_steam_appid_path(mcc_dir)
        try:
            with open(appid_path, "w", encoding="utf-8") as f:
                f.write("976730\n")
            logger.info(f"[Mod Manager] Configured Steam AppID: {appid_path}")
        except Exception as e:
            logger.warning(f"[Mod Manager] Could not write {appid_path}: {e}")

    return True


def cleanup_mod(mcc_dir: Optional[str] = None, force: bool = False) -> bool:
    """Remove dxgi.dll and steam_appid.txt from MCC's win64 directory."""
    if not mcc_dir:
        mcc_dir = detect_mcc_path()
    if not mcc_dir:
        return False

    dest = get_mcc_dxgi_path(mcc_dir)
    appid_path = get_mcc_steam_appid_path(mcc_dir)

    if not force and is_mcc_running():
        logger.warning(f"[Mod Manager] MCC is currently running. Skipping deletion of {dest}.")
        return False

    if os.path.exists(appid_path):
        try:
            os.remove(appid_path)
            logger.info(f"[Mod Manager] Removed {appid_path}.")
        except Exception:
            pass

    if not os.path.exists(dest):
        return True

    # Retry up to 5 times in case game just exited and Windows is releasing handle
    for attempt in range(1, 6):
        try:
            os.remove(dest)
            logger.info(f"[Mod Manager] Successfully removed {dest}. Normal gameplay safe.")
            return True
        except PermissionError:
            time.sleep(0.5)
        except FileNotFoundError:
            return True
        except Exception as e:
            logger.warning(f"[Mod Manager] Error removing {dest} (attempt {attempt}): {e}")
            time.sleep(0.5)

    logger.warning(f"[Mod Manager] Could not remove {dest} immediately (game may still be closing).")
    return False


def spawn_watchdog(mcc_pid: int, dxgi_path: str) -> None:
    """Spawn an independent, detached background process that waits for MCC to exit and deletes dxgi.dll.

    This ensures dxgi.dll is ALWAYS removed even if the Archipelago Client is abruptly terminated.
    """
    if mcc_pid <= 0 or not dxgi_path:
        return

    popen_kwargs: dict[str, Any] = {
        "close_fds": True,
        "stdin": subprocess.DEVNULL,
        "stdout": subprocess.DEVNULL,
        "stderr": subprocess.DEVNULL,
    }
    python_exe = sys.executable

    if sys.platform == "win32":
        # On Windows, prefer pythonw.exe if available so it runs as a native GUI subsystem
        # process without ever allocating a console window.
        dirname, filename = os.path.split(python_exe)
        if filename.lower() == "python.exe":
            pythonw = os.path.join(dirname, "pythonw.exe")
            if os.path.isfile(pythonw):
                python_exe = pythonw

        # Use CREATE_NO_WINDOW and hide the window via STARTUPINFO.
        # (Note: Win32 CreateProcess docs specify CREATE_NO_WINDOW must NOT be combined with DETACHED_PROCESS).
        popen_kwargs["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)

        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = subprocess.SW_HIDE
        popen_kwargs["startupinfo"] = startupinfo
    else:
        # On Linux/POSIX, start_new_session=True detaches the child from the controlling terminal
        # and session group via setsid().
        popen_kwargs["start_new_session"] = True

    try:
        subprocess.Popen(
            [python_exe, "-m", "worlds.halo3.cleanup", str(mcc_pid), dxgi_path],
            **popen_kwargs,
        )
        logger.info(f"[Watchdog] Detached cleanup supervisor spawned for MCC PID {mcc_pid}.")
    except Exception as e:
        logger.warning(f"[Watchdog] Could not spawn detached cleanup process: {e}")


def get_mcc_pid() -> int:
    """Return the PID of an active MCC-Win64-Shipping.exe process, or 0 if not found."""
    target_exes = {"mcc-win64-shipping.exe"}
    if sys.platform == "win32" and ctypes is not None:
        kernel32 = ctypes.windll.kernel32
        h_snap = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
        if h_snap == -1:
            return 0

        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", wintypes.DWORD),
                ("cntUsage", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", wintypes.DWORD),
                ("cntThreads", wintypes.DWORD),
                ("th32ParentProcessID", wintypes.DWORD),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", wintypes.DWORD),
                ("szExeFile", ctypes.c_char * 260),
            ]

        pe = PROCESSENTRY32()
        pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
        pid = 0

        if kernel32.Process32First(h_snap, ctypes.byref(pe)):
            while True:
                exe_name = pe.szExeFile.decode("latin1", errors="ignore").lower()
                if exe_name in target_exes:
                    pid = pe.th32ProcessID
                    break
                if not kernel32.Process32Next(h_snap, ctypes.byref(pe)):
                    break

        kernel32.CloseHandle(h_snap)
        return pid
    else:
        try:
            for pid_dir in os.listdir("/proc"):
                if not pid_dir.isdigit():
                    continue
                try:
                    cmdline_path = os.path.join("/proc", pid_dir, "cmdline")
                    if os.path.isfile(cmdline_path):
                        with open(cmdline_path, "rb") as f:
                            cmdline = f.read().decode("latin1", errors="ignore").lower()
                            if any(target in cmdline for target in target_exes):
                                return int(pid_dir)
                except (OSError, IOError):
                    continue
        except Exception:
            pass
        return 0


class LaunchedProcess:
    """Represents an active MCC process when launched externally (e.g. via Steam protocol URL)."""

    def __init__(self, pid: int, popen: Optional[subprocess.Popen] = None) -> None:
        self.pid = pid
        self._popen = popen

    def poll(self) -> Optional[int]:
        if self._popen:
            return self._popen.poll()
        return None if is_mcc_running() else 0

    def terminate(self) -> None:
        if self._popen:
            self._popen.terminate()

    def __bool__(self) -> bool:
        return self.pid > 0

    def __repr__(self) -> str:
        return f"<LaunchedProcess pid={self.pid}>"


def launch_mcc(mcc_dir: Optional[str] = None) -> Optional[subprocess.Popen | LaunchedProcess]:
    """Complete launch workflow:

    1. Detects MCC installation path.
    2. Sanitizes any stale mod DLL.
    3. Deploys bundled dxgi.dll.
    4. Launches MCC in Anti-Cheat Disabled mode (via Steam Option 2 or -no-eac / -eac-nop-loaded).
    5. Spawns the detached watchdog cleaner to guarantee post-game cleanup.
    """
    if not mcc_dir:
        mcc_dir = detect_mcc_path()

    if not mcc_dir:
        logger.error("[Launch] Could not locate Halo: The Master Chief Collection installation directory.")
        return None

    exe_path = os.path.join(mcc_dir, MCC_EXE_REL_PATH)
    if not os.path.isfile(exe_path):
        logger.error(f"[Launch] MCC-Win64-Shipping.exe not found at: {exe_path}")
        return None

    if is_mcc_running():
        logger.warning("[Launch] Halo: MCC is already running!")
        return None

    # Step 1: Pre-flight sanitization
    sanitize_mcc_directory(mcc_dir)

    # Step 2: Deploy mod DLL
    if not deploy_mod(mcc_dir):
        logger.error("[Launch] Aborting launch: failed to deploy dxgi.dll.")
        return None

    dxgi_dest = get_mcc_dxgi_path(mcc_dir)

    # Step 3: Launch MCC Anti-Cheat Disabled
    is_steam = "steam" in mcc_dir.lower()
    proc: Optional[subprocess.Popen | LaunchedProcess] = None
    mcc_pid: int = 0

    if is_steam:
        logger.info("==================================================================")
        logger.info("[Launch] Launching Halo: MCC via Steam (Option 2: Anti-Cheat DISABLED)...")
        logger.info("[Launch] URL: steam://launch/976730/option2")
        logger.info("==================================================================")
        try:
            if sys.platform == "win32":
                os.startfile("steam://launch/976730/option2")
            else:
                subprocess.Popen(["steam", "steam://launch/976730/option2"])
            # Wait for MCC process to initialize (up to 20 seconds)
            for _ in range(40):
                time.sleep(0.5)
                mcc_pid = get_mcc_pid()
                if mcc_pid > 0:
                    break
        except Exception as e:
            logger.warning(f"[Launch] steam://launch failed ({e}), falling back to direct executable...")

    if mcc_pid == 0 and not is_mcc_running():
        cmd = [exe_path, "-no-eac", "-eac-nop-loaded"]
        working_dir = os.path.join(mcc_dir, "mcc", "binaries", "win64")

        logger.info("==================================================================")
        logger.info("[Launch] Launching Halo: MCC direct executable with Anti-Cheat DISABLED...")
        logger.info(f"[Launch] Command: {cmd}")
        logger.info("==================================================================")

        try:
            proc = subprocess.Popen(cmd, cwd=working_dir)
            mcc_pid = proc.pid
        except Exception as e:
            logger.error(f"[Launch] Failed to launch MCC process: {e}")
            cleanup_mod(mcc_dir)
            return None

    # Step 4: Spawn the detached watchdog process
    if mcc_pid > 0:
        spawn_watchdog(mcc_pid, dxgi_dest)

    if proc is not None:
        return proc
    if mcc_pid > 0:
        return LaunchedProcess(mcc_pid)
    if is_mcc_running():
        active_pid = get_mcc_pid()
        if active_pid > 0:
            return LaunchedProcess(active_pid)

    return None
