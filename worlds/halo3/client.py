from __future__ import annotations
import argparse
import asyncio
import atexit
import json
import os
import sys
import typing

import Utils
from Utils import async_start

if __name__ == "__main__":
    import ModuleUpdate
    try:
        ModuleUpdate.update(yes=True)
    except Exception:
        pass
    Utils.init_logging("Halo3Client", exception_logger="Client")

from CommonClient import (
    CommonContext,
    ClientCommandProcessor,
    gui_enabled,
    logger,
    get_base_parser,
    server_loop,
)
from NetUtils import ClientStatus, NetworkItem

# Import world definitions & constants
from .constants import (
    MISSION_NAMES,
    SCENARIO_MAP,
    TIER_WEAPON_MAP,
    DEFAULT_PIPE_NAME as PIPE_NAME,
    DEFAULT_TCP_HOST,
    DEFAULT_TCP_PORT,
    MCC_SCENARIO_STRUCT_OFFSET,
    SCENARIO_STRING_OFFSET,
    PGCR_BASE_OFFSET,
    PGCR_INDICATOR_OFFSET,
)
from .locations import (
    LOCATION_NAME_TO_ID, MISSION_TO_LOCATION,
    SKULL_LOCATION_TO_REGION, TERMINAL_LOCATION_TO_REGION,
)
from .launcher import (
    detect_mcc_path,
    is_mcc_running,
    sanitize_mcc_directory,
    cleanup_mod,
    launch_mcc,
    get_mcc_dxgi_path,
    get_bundled_dxgi_path,
)


class Halo3CommandProcessor(ClientCommandProcessor):
    ctx: Halo3Context

    def __init__(self, ctx: Halo3Context) -> None:
        super().__init__(ctx)

    def _cmd_status(self) -> None:
        """Display current game status, unlocked missions, and MCC connection."""
        self.output("--- Halo 3 Archipelago Status ---")
        self.output(f"MCC Process Connected: {'YES' if self.ctx.mcc_connected else 'NO (searching...)'}")
        if self.ctx.current_mcc_map:
            self.output(f"Current MCC Map: {self.ctx.current_mcc_map}")
        
        self.output("\nUnlocked Missions:")
        for idx, mission in enumerate(MISSION_NAMES, start=1):
            unlocked = mission in self.ctx.unlocked_missions
            loc_name = MISSION_TO_LOCATION.get(mission, f"{mission} - Mission Complete")
            loc_id = LOCATION_NAME_TO_ID.get(loc_name)
            completed = loc_id in self.ctx.checked_locations if loc_id else False
            status_str = "[UNLOCKED]" if unlocked else "[LOCKED]"
            check_str = "[COMPLETED]" if completed else "[INCOMPLETE]"
            self.output(f"  {idx}. {mission:<16} {status_str:<12} {check_str}")

        if self.ctx.unlocked_weapons:
            self.output("\nUnlocked Weapon Licenses:")
            for weap in sorted(self.ctx.unlocked_weapons):
                self.output(f"  * {weap}")

        self.output("---------------------------------")

    def _cmd_complete(self, *args: str) -> None:
        """Manually mark a location or mission as completed. Usage: /complete <location name, mission, or ID>"""
        if not args:
            self.output("Please specify a location to complete. E.g.: /complete Sierra 117 or /complete Blind Skull")
            return

        arg_str = " ".join(args).strip()

        # 1. Direct ID check
        if arg_str.isdigit():
            val = int(arg_str)
            if val in LOCATION_NAME_TO_ID.values():
                loc_id = val
                loc_name = next(k for k, v in LOCATION_NAME_TO_ID.items() if v == loc_id)
                self.ctx.send_location_check(loc_id, loc_name)
                return
            elif 1 <= val <= len(MISSION_NAMES):
                matched_mission = MISSION_NAMES[val - 1]
                loc_name = MISSION_TO_LOCATION.get(matched_mission, f"{matched_mission} - Mission Complete")
                loc_id = LOCATION_NAME_TO_ID[loc_name]
                self.ctx.send_location_check(loc_id, matched_mission)
                return

        # 2. Check exact location name or partial match
        matched_loc_name = None
        for name in LOCATION_NAME_TO_ID:
            if arg_str.lower() == name.lower():
                matched_loc_name = name
                break
            if arg_str.lower() in name.lower():
                matched_loc_name = name

        if matched_loc_name:
            loc_id = LOCATION_NAME_TO_ID[matched_loc_name]
            if loc_id in self.ctx.checked_locations:
                self.output(f"'{matched_loc_name}' is already completed.")
                return
            self.ctx.send_location_check(loc_id, matched_loc_name)
            return

        self.output(f"Unknown location '{arg_str}'.")

    def _cmd_connect_mcc(self) -> None:
        """Attempt to immediately reconnect to Halo MCC process."""
        self.output("Scanning for MCC-Win64-Shipping.exe...")
        self.ctx.try_connect_mcc()

    def _cmd_launch(self, *args: str) -> None:
        """Deploy mod and launch Halo: MCC with Anti-Cheat disabled.
        Usage: /launch [mcc_directory_path]
        """
        custom_path = " ".join(args).strip() if args else None
        mcc_path = custom_path or detect_mcc_path()
        if not mcc_path:
            self.output("Could not detect MCC installation. Please specify path: /launch <path_to_mcc>")
            return
        if is_mcc_running():
            self.output("Halo: MCC is already running! Please close MCC before launching.")
            return
        self.output(f"Launching Halo: MCC from {mcc_path} with Anti-Cheat DISABLED...")
        proc = launch_mcc(mcc_path)
        if proc:
            self.output(f"Halo: MCC launched successfully (PID {proc.pid}). Watchdog cleanup supervisor active.")
        else:
            self.output("Failed to launch MCC. Check logs for details.")

    def _cmd_clean_mod(self, *args: str) -> None:
        """Remove dxgi.dll from MCC directory to restore clean vanilla state.
        Usage: /clean_mod [mcc_directory_path]
        """
        custom_path = " ".join(args).strip() if args else None
        mcc_path = custom_path or detect_mcc_path()
        if not mcc_path:
            self.output("Could not detect MCC installation directory.")
            return
        if is_mcc_running():
            self.output("WARNING: Halo: MCC is currently running! Windows locks dxgi.dll while the game is open.")
            self.output("Please close Halo: MCC first, or rely on the background watchdog which cleans it up automatically on exit.")
            return
        cleaned = cleanup_mod(mcc_path)
        if cleaned:
            self.output("Successfully removed dxgi.dll from MCC directory. Game is clean.")
        else:
            self.output("No mod DLL found in MCC directory, or already cleaned.")

    def _cmd_mod_status(self) -> None:
        """Check the status of the Halo 3 Archipelago mod and MCC installation."""
        mcc_dir = detect_mcc_path()
        self.output("--- Halo 3 Mod & MCC Status ---")
        self.output(f"Detected MCC Path: {mcc_dir or 'NOT FOUND'}")
        running = is_mcc_running()
        self.output(f"MCC Process Running: {'YES' if running else 'NO'}")
        if mcc_dir:
            dxgi_file = get_mcc_dxgi_path(mcc_dir)
            installed = os.path.isfile(dxgi_file)
            self.output(f"Mod DLL Installed in MCC: {'YES' if installed else 'NO'} ({dxgi_file})")
        bundled = get_bundled_dxgi_path()
        self.output(f"Bundled Mod DLL Available: {'YES' if os.path.isfile(bundled) else 'NO'} ({bundled})")
        self.output("Watchdog Supervisor: worlds/halo3/cleanup.py")
        self.output("--------------------------------")





class Halo3Context(CommonContext):
    command_processor = Halo3CommandProcessor
    game = "Halo 3"
    items_handling = 0b111  # receive all items

    def __init__(self, server_address: str, password: str | None) -> None:
        super().__init__(server_address, password)
        self.unlocked_missions: set[str] = set()
        self.unlocked_weapons: set[str] = set()
        self.weapon_license_mode: int = 1
        self.starting_weapon: str = "Assault Rifle"
        self.goal: int = 0
        self.finished_game: bool = False
        self.mcc_connected: bool = False
        self.current_mcc_map: str | None = None
        self.prev_mcc_map: str | None = None
        self.pm: typing.Any = None
        self.mcc_base: int | None = None
        self.halo3_module_base: int | None = None
        self.last_pgcr: bool = False
        self.pipe_handle: typing.Any = None
        self.tcp_sock: typing.Any = None
        self.ipc_connected: bool = False
        self.ipc_type: str = "none"

        # State bridge directory (cross-platform)
        if sys.platform == "win32":
            self.state_dir = os.path.expandvars(r"%localappdata%\Halo3AP")
        else:
            self.state_dir = os.path.expanduser("~/.local/share/Halo3AP")
        os.makedirs(self.state_dir, exist_ok=True)
        self.state_file = os.path.join(self.state_dir, "state.json")
        self.write_state_file()

    async def server_auth(self, password_requested: bool = False):
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    def connect_ipc(self) -> bool:
        """Connect to the native mod IPC server via TCP socket or Windows Named Pipe."""
        if self.ipc_connected and (self.tcp_sock is not None or self.pipe_handle is not None):
            return True

        # 1. Try Localhost TCP Socket (Universal across Windows, Linux, and Steam Deck)
        try:
            import socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(0.5)
            sock.connect((DEFAULT_TCP_HOST, DEFAULT_TCP_PORT))
            sock.settimeout(2.0)
            self.tcp_sock = sock
            self.ipc_connected = True
            self.ipc_type = "tcp"
            try:
                greeting = self.tcp_sock.recv(4096).decode(errors="ignore").strip()
                logger.info(f"[IPC] Connected to native mod via TCP: {greeting}")
            except Exception:
                logger.info("[IPC] Connected to native mod via TCP")
            return True
        except Exception:
            self.tcp_sock = None

        # 2. Fallback: Named Pipe on Windows
        if sys.platform == "win32":
            try:
                import win32file
                self.pipe_handle = win32file.CreateFile(
                    PIPE_NAME,
                    win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                    0,
                    None,
                    win32file.OPEN_EXISTING,
                    0,
                    None,
                )
                self.ipc_connected = True
                self.ipc_type = "pipe"
                try:
                    _, data = win32file.ReadFile(self.pipe_handle, 4096)
                    greeting = data.decode(errors="ignore").strip()
                    logger.info(f"[IPC] Connected to native mod via Named Pipe: {greeting}")
                except Exception:
                    logger.info("[IPC] Connected to native mod via Named Pipe")
                return True
            except Exception:
                self.pipe_handle = None

        self.ipc_connected = False
        self.ipc_type = "none"
        return False

    connect_pipe = connect_ipc  # Backwards compatibility alias

    def disconnect_ipc(self) -> None:
        self.ipc_connected = False
        self.ipc_type = "none"
        if self.tcp_sock is not None:
            try:
                self.tcp_sock.close()
            except Exception:
                pass
            self.tcp_sock = None
        if self.pipe_handle is not None:
            try:
                import win32file
                win32file.CloseHandle(self.pipe_handle)
            except Exception:
                pass
            self.pipe_handle = None

    def push_state_to_mod(self) -> None:
        """Push current unlock state to the native mod via TCP socket or Named Pipe."""
        if not self.ipc_connected:
            return
        state = {
            "unlocked_missions": list(self.unlocked_missions),
            "unlocked_weapons": list(self.unlocked_weapons),
            "weapon_license_mode": self.weapon_license_mode,
            "starting_weapon": self.starting_weapon,
        }
        payload = (json.dumps(state) + "\n").encode("utf-8")
        ack_str = None
        try:
            if self.ipc_type == "tcp" and self.tcp_sock is not None:
                self.tcp_sock.sendall(payload)
                ack_data = self.tcp_sock.recv(4096)
                if not ack_data:
                    raise ConnectionResetError("TCP socket closed by remote")
                ack_str = ack_data.decode(errors="ignore").strip()
            elif self.ipc_type == "pipe" and self.pipe_handle is not None:
                import win32file
                win32file.WriteFile(self.pipe_handle, payload)
                _, ack_data = win32file.ReadFile(self.pipe_handle, 4096)
                ack_str = ack_data.decode(errors="ignore").strip()
        except Exception as e:
            logger.debug(f"[IPC] State push failed: {e}")
            self.disconnect_ipc()
            return

        if ack_str:
            try:
                sanitized_ack = ack_str.replace('\\', '/')
                ack_json = json.loads(sanitized_ack)

                # Check if mod reported current map
                mod_map = ack_json.get("current_map")
                if mod_map:
                    detected_name = None
                    for code, name in SCENARIO_MAP.items():
                        if code in mod_map:
                            detected_name = name
                            break
                    if detected_name and self.current_mcc_map != detected_name:
                        self.prev_mcc_map = self.current_mcc_map
                        self.current_mcc_map = detected_name
                        logger.info(f"[MCC Mod Sync] Active Level: {detected_name}")

                events = ack_json.get("events", [])
                if events:
                    logger.debug(f"[IPC] Mod response with events: {ack_str}")
                    self.process_mod_events(events)
            except json.JSONDecodeError as err:
                logger.error(f"[IPC] Failed to parse JSON response from mod: {err} (raw: {ack_str})")

    def process_mod_events(self, events: list[str]) -> None:
        """Process events detected by the native mod (rally points, terminals, skulls, mission completion).

        Event format:
            rally_1        -> Rally Point Bravo of current mission
            rally_2        -> Rally Point Charlie of current mission
            terminal_N     -> Terminal N of current mission
            skull_primary_N   -> Primary skull N collected
            skull_secondary_N -> Secondary skull N collected
            game_won       -> Mission completed (auto-return to menu triggered by mod)
        """
        for event in events:
            logger.info(f"[Mod Event] Processing: {event}")

            if event == "game_won" or event.startswith("game_won:"):
                self._handle_game_won_event(event)
            elif event.startswith("rally_"):
                self._handle_rally_point_event(event)
            elif event.startswith("terminal_"):
                self._handle_terminal_event(event)
            elif event.startswith("skull_"):
                self._handle_skull_event(event)
            else:
                logger.warning(f"[Mod Event] Unknown event type: {event}")

    def _handle_rally_point_event(self, event: str) -> None:
        """Map rally_N events to location checks based on current mission."""
        target_map = None
        if ":" in event:
            parts = event.split(":", 1)
            event_name = parts[0]
            scen_tag = parts[1]
            for code, name in SCENARIO_MAP.items():
                if code in scen_tag:
                    target_map = name
                    break
        else:
            event_name = event

        if not target_map:
            if self.current_mcc_map and self.current_mcc_map in MISSION_NAMES:
                target_map = self.current_mcc_map
            elif self.prev_mcc_map and self.prev_mcc_map in MISSION_NAMES:
                target_map = self.prev_mcc_map

        if not target_map:
            logger.warning(f"[Rally Point] Event {event} received but no active mission known")
            return

        try:
            index = int(event_name.split("_")[1])
        except (IndexError, ValueError):
            logger.warning(f"[Rally Point] Could not parse index from: {event}")
            return

        rally_name = "Bravo" if index == 1 else "Charlie" if index == 2 else f"Unknown({index})"
        loc_name = f"{target_map} - Rally Point {rally_name}"

        loc_id = LOCATION_NAME_TO_ID.get(loc_name)
        if loc_id and loc_id not in self.checked_locations:
            logger.info(f"[Rally Point] Reached {loc_name}!")
            self.send_location_check(loc_id, loc_name)
        elif not loc_id:
            logger.debug(f"[Rally Point] No location ID for: {loc_name}")

    def _handle_terminal_event(self, event: str) -> None:
        """Map terminal_N events to location checks based on current mission.

        Terminal indices are per-map. We need to find the N-th terminal in the
        current mission and map it to the correct location."""
        target_map = None
        if ":" in event:
            parts = event.split(":", 1)
            event_name = parts[0]
            scen_tag = parts[1]
            for code, name in SCENARIO_MAP.items():
                if code in scen_tag:
                    target_map = name
                    break
        else:
            event_name = event

        if not target_map:
            if self.current_mcc_map and self.current_mcc_map in MISSION_NAMES:
                target_map = self.current_mcc_map
            elif self.prev_mcc_map and self.prev_mcc_map in MISSION_NAMES:
                target_map = self.prev_mcc_map

        if not target_map:
            logger.warning(f"[Terminal] Event {event} received but no active mission known")
            return

        try:
            terminal_index = int(event_name.split("_")[1])
        except (IndexError, ValueError):
            logger.warning(f"[Terminal] Could not parse index from: {event}")
            return

        # Find terminals for the current mission and match by index
        mission_terminals = [
            name for name, region in TERMINAL_LOCATION_TO_REGION.items()
            if region == target_map
        ]
        mission_terminals.sort()  # Ensure consistent ordering

        # Terminal indices are 0-based from the mod
        if 0 <= terminal_index < len(mission_terminals):
            loc_name = mission_terminals[terminal_index]
        elif terminal_index < 10:  # Try 1-based
            adj_idx = terminal_index - 1
            if 0 <= adj_idx < len(mission_terminals):
                loc_name = mission_terminals[adj_idx]
            else:
                logger.warning(f"[Terminal] Index {terminal_index} out of range for {target_map} (has {len(mission_terminals)} terminals)")
                return
        else:
            logger.warning(f"[Terminal] Index {terminal_index} out of range for {target_map}")
            return

        loc_id = LOCATION_NAME_TO_ID.get(loc_name)
        if loc_id and loc_id not in self.checked_locations:
            logger.info(f"[Terminal] Accessed {loc_name}!")
            self.send_location_check(loc_id, loc_name)

    def _handle_skull_event(self, event: str) -> None:
        """Map skull_primary_N / skull_secondary_N events to location checks.

        In Halo 3, each campaign mission has at most one primary skull and at most
        one secondary skull. We resolve the location by matching the current mission
        and skull type (primary vs secondary), which is 100% reliable across all
        engine versions and avoids index mismatches.
        """
        target_map = None
        if ":" in event:
            parts = event.split(":", 1)
            event_name = parts[0]
            scen_tag = parts[1]
            for code, name in SCENARIO_MAP.items():
                if code in scen_tag:
                    target_map = name
                    break
        else:
            event_name = event

        parts = event_name.split("_")
        if len(parts) < 3:
            logger.warning(f"[Skull] Could not parse: {event}")
            return

        skull_type = parts[1]  # 'primary' or 'secondary'
        try:
            skull_index = int(parts[2])
        except ValueError:
            logger.warning(f"[Skull] Could not parse index from: {event}")
            return

        SECONDARY_SKULL_NAMES = {
            "Blind Skull",
            "Grunt Birthday Party Skull",
            "Cowbell Skull",
            "IWHBYD Skull",
        }

        loc_name = None
        # Check resolved mission first, then current/previous
        candidate_maps = [m for m in (target_map, self.current_mcc_map, self.prev_mcc_map) if m]
        for c_map in candidate_maps:
            for name, region in SKULL_LOCATION_TO_REGION.items():
                if region == c_map:
                    is_sec = any(sec in name for sec in SECONDARY_SKULL_NAMES)
                    if (skull_type == "secondary" and is_sec) or (skull_type == "primary" and not is_sec):
                        loc_name = name
                        break
            if loc_name:
                break

        # Fallback to index mapping if mission wasn't recognized
        if not loc_name:
            PRIMARY_SKULLS = {
                0: "Iron Skull", 1: "Black Eye Skull", 2: "Tough Luck Skull",
                3: "Catch Skull", 4: "Fog Skull", 5: "Famine Skull",
                6: "Thunderstorm Skull", 7: "Tilt Skull", 8: "Mythic Skull",
            }
            SECONDARY_SKULLS = {
                0: "Blind Skull", 1: "Grunt Birthday Party Skull",
                2: "Cowbell Skull", 3: "IWHBYD Skull",
            }
            skull_map = PRIMARY_SKULLS if skull_type == "primary" else SECONDARY_SKULLS
            skull_name = skull_map.get(skull_index)
            if skull_name:
                for name in SKULL_LOCATION_TO_REGION:
                    if skull_name in name:
                        loc_name = name
                        break

        if loc_name:
            loc_id = LOCATION_NAME_TO_ID.get(loc_name)
            if loc_id and loc_id not in self.checked_locations:
                logger.info(f"[Skull] Collected {loc_name}!")
                self.send_location_check(loc_id, loc_name)
            else:
                logger.info(f"[Skull] Location already checked: {loc_name}")
        else:
            logger.warning(f"[Skull] No location found for {skull_type} skull (index {skull_index}) on {target_map or self.current_mcc_map}")

    def _handle_game_won_event(self, event: str = "game_won") -> None:
        """Handle mission completion event from the native mod.

        If the game already began loading the next mission before the event was
        read from the pipe, credit the previous mission rather than the newly loading one.
        """
        target_map = None
        if ":" in event:
            parts = event.split(":", 1)
            scen_tag = parts[1]
            for code, name in SCENARIO_MAP.items():
                if code in scen_tag:
                    target_map = name
                    break

        if not target_map:
            target_map = self.prev_mcc_map if (self.prev_mcc_map and self.prev_mcc_map in MISSION_TO_LOCATION) else self.current_mcc_map

        if not target_map:
            logger.warning("[Game Won] Mission completed but no current or previous map known!")
            return

        logger.info(f"[Game Won] Mission complete: {target_map}")

        loc_name = MISSION_TO_LOCATION.get(target_map)
        if loc_name:
            loc_id = LOCATION_NAME_TO_ID.get(loc_name)
            if loc_id and loc_id not in self.checked_locations:
                logger.info(f"[Game Won] Sending location check: {loc_name}")
                self.send_location_check(loc_id, target_map)
            else:
                logger.info(f"[Game Won] Location already checked: {loc_name}")
                self.check_goal_condition(loc_id)
        else:
            logger.warning(f"[Game Won] No location mapping for mission: {target_map}")

    def check_goal_condition(self, loc_id: int | None = None) -> None:
        if not self.finished_game:
            if self.goal == 0:  # Beat Halo
                halo_id = LOCATION_NAME_TO_ID.get("Halo - Mission Complete")
                if loc_id == halo_id or (halo_id and halo_id in self.checked_locations):
                    logger.info("[AP] Victory condition met! Completed Halo!")
                    async_start(self.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}]))
                    self.finished_game = True
            elif self.goal == 1:  # Beat All Missions
                all_mission_loc_ids = {
                    LOCATION_NAME_TO_ID[f"{m} - Mission Complete"] for m in MISSION_NAMES
                }
                if all_mission_loc_ids.issubset(self.checked_locations):
                    logger.info("[AP] Victory condition met! All 9 missions completed!")
                    async_start(self.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}]))
                    self.finished_game = True

    def write_state_file(self) -> None:
        data = {
            "unlocked_missions": list(self.unlocked_missions),
            "unlocked_weapons": list(self.unlocked_weapons),
            "checked_locations": list(self.checked_locations),
            "mcc_connected": self.mcc_connected,
            "current_map": self.current_mcc_map,
        }
        try:
            with open(self.state_file, "w") as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            logger.debug(f"Failed to write state file: {e}")
        # Also push to native mod via pipe
        self.push_state_to_mod()

    def on_package(self, cmd: str, args: dict[str, typing.Any]) -> None:
        if cmd == "Connected":
            logger.info("Connected to Archipelago server!")
            slot_data = args.get("slot_data", {})
            self.weapon_license_mode = slot_data.get("weapon_license_mode", 1)
            self.starting_weapon = slot_data.get("starting_weapon", "assault_rifle")
            self.goal = slot_data.get("goal", 0)

            # Reconcile checked locations from server on reconnect
            server_checked = args.get("checked_locations", [])
            if server_checked:
                self.checked_locations |= set(server_checked)
                logger.info(f"[AP] Reconciled {len(server_checked)} checked locations from server.")
                self.check_goal_condition()

            self.write_state_file()

        elif cmd == "ReceivedItems":
            for item in args.get("items", []):
                net_item = NetworkItem(*item)
                item_name = self.item_names.lookup_in_slot(net_item.item, self.slot)
                if item_name and "Unlock" in item_name:
                    mission_name = item_name.replace(" Unlock", "").strip()
                    if mission_name not in self.unlocked_missions:
                        self.unlocked_missions.add(mission_name)
                        logger.info(f"[AP] >> UNLOCKED MISSION: {mission_name}! <<")
                elif item_name and "License" in item_name:
                    raw_name = item_name.replace(" License", "").strip()
                    if raw_name in TIER_WEAPON_MAP:
                        for w in TIER_WEAPON_MAP[raw_name]:
                            if w not in self.unlocked_weapons:
                                self.unlocked_weapons.add(w)
                                logger.info(f"[AP] >> UNLOCKED WEAPON (via {raw_name}): {w}! <<")
                    else:
                        norm_name = raw_name.lower().replace(" ", "_").replace("-", "_")
                        if norm_name not in self.unlocked_weapons:
                            self.unlocked_weapons.add(norm_name)
                            logger.info(f"[AP] >> UNLOCKED WEAPON: {norm_name}! <<")
            self.write_state_file()

        elif cmd == "RoomUpdate":
            self.write_state_file()


    def send_location_check(self, loc_id: int, check_name: str) -> None:
        self.checked_locations.add(loc_id)
        async_start(self.send_msgs([{"cmd": "LocationChecks", "locations": [loc_id]}]))
        logger.info(f"[AP] Completed check: {check_name}")
        self.write_state_file()

        # Check goal condition
        self.check_goal_condition(loc_id)

    def try_connect_mcc(self) -> bool:
        if self.ipc_connected:
            self.mcc_connected = True
            return True

        if sys.platform == "win32":
            try:
                import pymem
                import pymem.process
                self.pm = pymem.Pymem("MCC-Win64-Shipping.exe")
                self.mcc_connected = True
                logger.info("Successfully hooked MCC-Win64-Shipping.exe!")

                mcc_mod = pymem.process.module_from_name(self.pm.process_handle, "MCC-Win64-Shipping.exe")
                if mcc_mod:
                    self.mcc_base = mcc_mod.lpBaseOfDll

                # Check for halo3.dll
                module = pymem.process.module_from_name(self.pm.process_handle, "halo3.dll")
                if module:
                    self.halo3_module_base = module.lpBaseOfDll
                    logger.info(f"halo3.dll detected at base address: 0x{self.halo3_module_base:X}")
                else:
                    self.halo3_module_base = None
                    logger.info("MCC running, waiting for Halo 3 to be loaded (halo3.dll)...")
                return True
            except Exception:
                self.pm = None
                self.mcc_connected = False
                self.mcc_base = None
                self.halo3_module_base = None
                return False
        return False

    def run_gui(self) -> None:
        from kvui import GameManager

        class Halo3Manager(GameManager):
            logging_pairs = [("Client", "Archipelago")]
            base_title = "Archipelago Halo 3 Client"

        self.ui = Halo3Manager(self)
        self.ui_task = asyncio.create_task(self.ui.async_run(), name="UI")


async def mcc_watcher(ctx: Halo3Context) -> None:
    logger.info("Halo 3 MCC Watcher started.")
    logger.info(f"Bridge directory: {ctx.state_dir}")

    # Check for external trigger file (e.g. trigger_complete.txt written by Cheat Engine)
    trigger_file = os.path.join(ctx.state_dir, "complete_mission.txt")

    while not ctx.exit_event.is_set():
        # Check if external bridge file requests a mission completion
        if os.path.exists(trigger_file):
            try:
                with open(trigger_file, "r") as f:
                    content = f.read().strip()
                os.remove(trigger_file)
                if content in MISSION_TO_LOCATION:
                    loc_name = MISSION_TO_LOCATION[content]
                    loc_id = LOCATION_NAME_TO_ID[loc_name]
                    if loc_id not in ctx.checked_locations:
                        ctx.send_location_check(loc_id, content)
            except Exception as e:
                logger.debug(f"Error handling trigger file: {e}")

        # Check MCC IPC connection (TCP socket or pipe)
        if not ctx.ipc_connected:
            if ctx.connect_ipc():
                ctx.mcc_connected = True
                ctx.push_state_to_mod()
            elif not ctx.mcc_connected:
                ctx.try_connect_mcc()
        else:
            ctx.push_state_to_mod()

        if ctx.mcc_connected and ctx.pm is not None:
            # Check if process is still alive and read live status
            try:
                import pymem.process
                if not ctx.halo3_module_base:
                    mod = pymem.process.module_from_name(ctx.pm.process_handle, "halo3.dll")
                    if mod:
                        ctx.halo3_module_base = mod.lpBaseOfDll
                        logger.info(f"halo3.dll loaded at: 0x{ctx.halo3_module_base:X}")

                # Automatic memory inspection
                if ctx.mcc_base:
                    # 1. Read scenario path from MCC scenario struct
                    # Base offset MCC_SCENARIO_STRUCT_OFFSET -> struct pointer -> struct+SCENARIO_STRING_OFFSET
                    try:
                        struct_addr = ctx.pm.read_longlong(ctx.mcc_base + MCC_SCENARIO_STRUCT_OFFSET)
                        if struct_addr:
                            raw_scenario = ctx.pm.read_string(struct_addr + SCENARIO_STRING_OFFSET, 64)
                            detected_map = None
                            for code, name in SCENARIO_MAP.items():
                                if code in raw_scenario:
                                    detected_map = name
                                    break

                            if detected_map and ctx.current_mcc_map != detected_map:
                                prev_map = ctx.current_mcc_map
                                ctx.prev_mcc_map = prev_map
                                ctx.current_mcc_map = detected_map
                                logger.info(f"[MCC Auto-Detect] Active Level: {detected_map}")
                                ctx.write_state_file()

                                # Level transition auto-completion:
                                # In MCC Campaign, finishing a level automatically loads the next level.
                                # If transitioning from one mission to another, the previous mission was completed!
                                if prev_map and prev_map in MISSION_TO_LOCATION and detected_map in SCENARIO_MAP.values():
                                    loc_name = MISSION_TO_LOCATION.get(prev_map)
                                    if loc_name:
                                        loc_id = LOCATION_NAME_TO_ID.get(loc_name)
                                        if loc_id and loc_id not in ctx.checked_locations:
                                            logger.info(f"[MCC Auto-Detect] Mission Transition Detected! Completed: {prev_map}")
                                            ctx.send_location_check(loc_id, prev_map)

                                # Log if player is on a locked level (native mod handles actual gating)
                                if ctx.unlocked_missions and detected_map not in ctx.unlocked_missions:
                                    logger.warning(f"[AP] Note: '{detected_map}' is not unlocked (native mod should gate this)")
                    except Exception:
                        pass

                    # 2. Check PGCR indicator (Post-Game Carnage Report = mission finished)
                    # menustate = PGCR_BASE_OFFSET; pgcr = menustate + PGCR_INDICATOR_OFFSET
                    try:
                        pgcr = ctx.pm.read_uchar(ctx.mcc_base + PGCR_BASE_OFFSET + PGCR_INDICATOR_OFFSET)
                        if pgcr == 1 and not ctx.last_pgcr and ctx.current_mcc_map:
                            logger.info(f"[MCC Auto-Detect] Post-Game Carnage Report detected! Mission Complete: {ctx.current_mcc_map}")
                            loc_name = MISSION_TO_LOCATION.get(ctx.current_mcc_map)
                            if loc_name:
                                loc_id = LOCATION_NAME_TO_ID[loc_name]
                                if loc_id not in ctx.checked_locations:
                                    ctx.send_location_check(loc_id, ctx.current_mcc_map)

                            logger.info("[AP] Mission complete! Return to Campaign Menu when ready.")
                        ctx.last_pgcr = (pgcr == 1)
                    except Exception:
                        pass

            except Exception:
                ctx.mcc_connected = False
                ctx.pm = None
                ctx.mcc_base = None
                ctx.halo3_module_base = None
                logger.info("Lost connection to MCC-Win64-Shipping.exe.")

        await asyncio.sleep(0.5)


async def main(args: argparse.Namespace) -> None:
    # Pre-flight sanitization: if MCC isn't running, ensure no stale dxgi.dll was left behind
    mcc_path = detect_mcc_path()
    if mcc_path and not is_mcc_running():
        cleaned = sanitize_mcc_directory(mcc_path)
        if cleaned:
            logger.info(f"[Safety] Sanitized stale mod DLL from {mcc_path}")

    # Auto-launch MCC if requested via --launch CLI argument
    if getattr(args, "launch", False):
        logger.info("[Auto-Launch] --launch flag supplied: launching Halo: MCC...")
        proc = launch_mcc(mcc_path)
        if proc:
            logger.info(f"[Auto-Launch] Halo: MCC launched with PID {proc.pid}.")
        else:
            logger.warning("[Auto-Launch] Failed to launch Halo: MCC. Check logs.")

    ctx = Halo3Context(args.connect, args.password)
    if args.name:
        ctx.auth = args.name
    ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")

    if gui_enabled:
        ctx.run_gui()
    ctx.run_cli()

    watcher_task = asyncio.create_task(mcc_watcher(ctx), name="Halo3Watcher")

    await ctx.exit_event.wait()
    await watcher_task
    await ctx.shutdown()


def parse_args(argv: typing.Sequence[str] | None = None) -> argparse.Namespace:
    parser = get_base_parser(description="Halo 3 Archipelago Client")
    parser.add_argument("patch_file", default="", nargs="?", help="Path to an .aphalo3 patch file.")
    parser.add_argument("--name", default=None, help="Slot name to connect as.")
    parser.add_argument("--launch", "-l", action="store_true", help="Deploy mod and launch Halo: MCC with Anti-Cheat disabled.")
    args, _ = parser.parse_known_args(argv)

    # If an .aphalo3 patch file is supplied, read archipelago.json metadata
    if getattr(args, "patch_file", None) and os.path.isfile(args.patch_file):
        try:
            import zipfile
            with zipfile.ZipFile(args.patch_file, "r") as zf:
                if "archipelago.json" in zf.namelist():
                    manifest = json.loads(zf.read("archipelago.json").decode("utf-8"))
                    if not args.name and manifest.get("player_name"):
                        args.name = manifest["player_name"]
                    if not args.connect and manifest.get("server"):
                        args.connect = manifest["server"]
                    logger.info(f"[Patch] Loaded '{args.patch_file}' for player '{manifest.get('player_name')}'.")
            # Opening a patch file implies launching into the game
            args.launch = True
        except Exception as e:
            logger.warning(f"[Patch] Could not read container {args.patch_file}: {e}")

    return args


def launch(*launch_args: str) -> None:
    """Synchronous launcher entry point compatible with LauncherComponents and CLI."""
    import colorama
    atexit.register(cleanup_mod)
    args = parse_args(launch_args if launch_args else None)
    colorama.init()
    asyncio.run(main(args))
    colorama.deinit()


if __name__ == "__main__":
    launch()


