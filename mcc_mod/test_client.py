"""
Interactive Test Client for Halo 3 Archipelago Native Mod.
Allows testing mission gating, weapon gating, and starter loadout in real-time.
"""

import time
import json
import threading
import os
import win32file

PIPE_NAME = r"\\.\pipe\Halo3Archipelago"

# Default test state
current_state = {
    "unlocked_missions": ["010_jungle"],
    "unlocked_weapons": ["assault_rifle"],
    "weapon_license_mode": 1,          # 1 = individual
    "starting_weapon": "assault_rifle"
}

def log_tailer_thread():
    """Tails %TEMP%\\Halo3AP_mod.log in the background, including recent startup lines."""
    log_path = os.path.expandvars(r"%TEMP%\Halo3AP_mod.log")
    # Start 4KB before EOF so startup messages from game boot are visible
    file_size = os.path.getsize(log_path) if os.path.exists(log_path) else 0
    last_size = max(0, file_size - 4096)

    while True:
        time.sleep(0.3)
        if os.path.exists(log_path):
            cur_size = os.path.getsize(log_path)
            if cur_size > last_size:
                with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
                    f.seek(last_size)
                    new_lines = f.read()
                    print(new_lines, end="", flush=True)
                last_size = cur_size

def send_state(handle):
    payload = json.dumps(current_state).encode()
    win32file.WriteFile(handle, payload)
    _, ack = win32file.ReadFile(handle, 4096)

def main():
    print("=" * 65)
    print(" [*] Halo 3 Archipelago - Interactive Live Testing Console")
    print("=" * 65)
    print(f"Connecting to Named Pipe: {PIPE_NAME}...")
    print("(Ensure MCC is running with Anti-Cheat Disabled)")

    while True:
        try:
            handle = win32file.CreateFile(
                PIPE_NAME,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None
            )
            print("[+] Connected to Halo 3 Archipelago Native Mod!")
            break
        except Exception:
            time.sleep(1.0)

    # Read greeting
    try:
        _, data = win32file.ReadFile(handle, 4096)
        print(f"[Mod] {data.decode().strip()}")
    except Exception:
        pass

    # Send initial state
    send_state(handle)

    # Start background log tailer
    tailer = threading.Thread(target=log_tailer_thread, daemon=True)
    tailer.start()

    print("\n" + "=" * 65)
    print(" [LIVE TEST READY]")
    print(f"   Unlocked Missions: {current_state['unlocked_missions']}")
    print(f"   Unlocked Weapons : {current_state['unlocked_weapons']}")
    print(f"   Starter Weapon   : {current_state['starting_weapon']}")
    print("=" * 65)
    print(" Commands:")
    print("   unlock <weapon>     - Unlock a weapon (e.g. 'unlock plasma_pistol', 'unlock magnum')")
    print("   lock <weapon>       - Lock a weapon (e.g. 'lock assault_rifle')")
    print("   starter <weapon>    - Set starting weapon (e.g. 'starter plasma_pistol')")
    print("   unlock all          - Unlock all 19 weapons")
    print("   lock all            - Lock all weapons except starter")
    print("   status              - Display current state")
    print("   quit                - Exit")
    print("=" * 65 + "\n")

    ALL_WEAPONS = [
        "assault_rifle", "battle_rifle", "magnum", "plasma_pistol", "plasma_rifle",
        "spiker", "needler", "mauler", "shotgun", "smg", "sniper_rifle",
        "beam_rifle", "covenant_carbine", "brute_shot", "rocket_launcher",
        "flak_cannon", "spartan_laser", "energy_sword", "gravity_hammer",
        "sentinel_beam", "missile_pod"
    ]

    while True:
        try:
            cmd = input().strip()
            if not cmd:
                continue

            parts = cmd.split(maxsplit=1)
            action = parts[0].lower()
            arg = parts[1].strip().lower().replace(" ", "_") if len(parts) > 1 else ""

            if action == "quit" or action == "exit":
                break
            elif action == "status":
                print(f"Missions: {current_state['unlocked_missions']}")
                print(f"Weapons : {current_state['unlocked_weapons']}")
                print(f"Starter : {current_state['starting_weapon']}")
            elif action == "unlock":
                if arg == "all":
                    current_state["unlocked_weapons"] = list(ALL_WEAPONS)
                    send_state(handle)
                    print("[+] Unlocked ALL weapons!")
                elif arg in ALL_WEAPONS or arg:
                    if arg not in current_state["unlocked_weapons"]:
                        current_state["unlocked_weapons"].append(arg)
                    send_state(handle)
                    print(f"[+] Unlocked '{arg}'. Active: {current_state['unlocked_weapons']}")
            elif action == "lock":
                if arg == "all":
                    current_state["unlocked_weapons"] = [current_state["starting_weapon"]]
                    send_state(handle)
                    print(f"[-] Locked all weapons except starter '{current_state['starting_weapon']}'.")
                elif arg in current_state["unlocked_weapons"]:
                    current_state["unlocked_weapons"].remove(arg)
                    send_state(handle)
                    print(f"[-] Locked '{arg}'. Active: {current_state['unlocked_weapons']}")
            elif action == "starter":
                if arg in ALL_WEAPONS:
                    current_state["starting_weapon"] = arg
                    if arg not in current_state["unlocked_weapons"]:
                        current_state["unlocked_weapons"].append(arg)
                    send_state(handle)
                    print(f"[+] Set starter weapon to '{arg}'!")
                else:
                    print(f"[!] Unknown weapon '{arg}'. Choose from: {ALL_WEAPONS}")
            else:
                print(f"[!] Unknown command '{cmd}'. Available: unlock, lock, starter, status, quit")

        except (KeyboardInterrupt, EOFError):
            break

    win32file.CloseHandle(handle)
    print("\n[*] Disconnected.")

if __name__ == "__main__":
    main()

