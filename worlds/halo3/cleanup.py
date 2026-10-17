from __future__ import annotations
import ctypes
import os
import sys
import time

LOG_PATH = os.path.join(os.environ.get("TEMP", "."), "Halo3AP_cleanup.log")


def log(msg: str) -> None:
    timestamp = time.strftime("[%Y-%m-%d %H:%M:%S]")
    line = f"{timestamp} {msg}\n"
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(line)
    except Exception:
        pass


def is_mcc_running() -> bool:
    """Return True if any MCC process is currently active."""
    target_exes = {b"mcc-win64-shipping.exe"}
    if sys.platform == "win32":
        import ctypes.wintypes
        kernel32 = ctypes.windll.kernel32
        h_snap = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
        if h_snap == -1:
            return False

        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", ctypes.wintypes.DWORD),
                ("cntUsage", ctypes.wintypes.DWORD),
                ("th32ProcessID", ctypes.wintypes.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", ctypes.wintypes.DWORD),
                ("cntThreads", ctypes.wintypes.DWORD),
                ("th32ParentProcessID", ctypes.wintypes.DWORD),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", ctypes.wintypes.DWORD),
                ("szExeFile", ctypes.c_char * 260),
            ]

        pe = PROCESSENTRY32()
        pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
        found = False

        if kernel32.Process32First(h_snap, ctypes.byref(pe)):
            while True:
                if pe.szExeFile.lower() in target_exes:
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
                            cmdline = f.read().lower()
                            if any(target in cmdline for target in target_exes):
                                return True
                except (OSError, IOError):
                    continue
        except Exception:
            pass
        return False


def wait_for_mcc_exit(initial_pid: int = 0) -> None:
    """Block until MCC terminates.
    
    Steam launches MCC by spawning an initial process that immediately handshakes
    and hands off to the main shipping executable (a new PID).
    The watchdog ensures MCC has fully started and is active, then continuously
    monitors until the main shipping executable has completely exited.
    """
    # 1. Give MCC a grace period to start up if not already running
    if not is_mcc_running():
        log(f"Waiting for MCC shipping executable to become active (initial PID {initial_pid})...")
        startup_timeout = 30.0
        start_time = time.time()
        while not is_mcc_running() and (time.time() - start_time) < startup_timeout:
            time.sleep(0.5)

    if not is_mcc_running():
        log("[WARN] MCC shipping executable was never detected active within startup window.")
        return

    # 2. If an initial PID was given and valid, wait for it if still alive
    if initial_pid > 0 and sys.platform == "win32":
        kernel32 = ctypes.windll.kernel32
        h_proc = kernel32.OpenProcess(0x00100000, False, initial_pid)
        if h_proc:
            try:
                log(f"Watchdog attached to initial MCC PID {initial_pid}. Waiting...")
                kernel32.WaitForSingleObject(h_proc, 0xFFFFFFFF)
                log(f"Initial MCC PID {initial_pid} completed.")
            finally:
                kernel32.CloseHandle(h_proc)

    # 3. Monitor until all MCC shipping executables have exited
    log("MCC shipping process confirmed active. Monitoring until game exit...")
    while is_mcc_running():
        time.sleep(1.0)

    # Re-verify after a brief pause to prevent race condition during game shutdown
    time.sleep(2.0)
    while is_mcc_running():
        time.sleep(1.0)

    log("All MCC processes have terminated.")


def remove_mod_file(dxgi_path: str, max_retries: int = 20, delay_sec: float = 0.5) -> bool:
    """Safely remove dxgi.dll, retrying while Windows releases the file handle."""
    if is_mcc_running():
        log(f"[WARN] MCC is still running! Refusing to delete {dxgi_path}.")
        return False

    # Also remove steam_appid.txt if present
    appid_path = os.path.join(os.path.dirname(dxgi_path), "steam_appid.txt")
    if os.path.exists(appid_path):
        try:
            os.remove(appid_path)
            log(f"[SUCCESS] Removed steam_appid.txt: {appid_path}")
        except Exception as e:
            log(f"[WARN] Error removing steam_appid.txt: {e}")

    if not os.path.exists(dxgi_path):
        log(f"Mod file already absent: {dxgi_path}")
        return True

    for attempt in range(1, max_retries + 1):
        try:
            os.remove(dxgi_path)
            log(f"[SUCCESS] Removed mod DLL on attempt {attempt}: {dxgi_path}")
            return True
        except PermissionError:
            time.sleep(delay_sec)
        except FileNotFoundError:
            log(f"[SUCCESS] Mod DLL removed by another process: {dxgi_path}")
            return True
        except Exception as e:
            log(f"[WARN] Error removing {dxgi_path} (attempt {attempt}): {e}")
            time.sleep(delay_sec)

    log(f"[ERROR] Failed to remove mod DLL after {max_retries} attempts: {dxgi_path}")
    return False


def main() -> int:
    if len(sys.argv) < 3:
        log("Usage: python -m worlds.halo3.cleanup <mcc_pid> <dxgi_path>")
        return 1

    try:
        pid = int(sys.argv[1])
    except ValueError:
        pid = 0

    dxgi_path = sys.argv[2]
    log(f"Starting standalone watchdog cleaner for target PID {pid}, file: {dxgi_path}")

    wait_for_mcc_exit(pid)
    success = remove_mod_file(dxgi_path)
    log(f"Watchdog cleanup completed. Result: {'OK' if success else 'FAILED'}")
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
