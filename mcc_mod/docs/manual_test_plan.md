# Halo 3 Archipelago Integration - Master Manual Test Plan

This manual test plan provides comprehensive, step-by-step verification procedures for every component of the Halo 3 Archipelago implementation:
- **Seed Generation & Containers (`.aphalo3`)**
- **Automated Mod Deployment & Pre-Flight Sanitization**
- **Detached Watchdog Process & Guaranteed Post-Game Cleanup**
- **In-DLL Anti-Cheat Safety Failsafe (0 Ban Risk)**
- **Named Pipe IPC & Client Synchronization (`Halo3Client.py`)**
- **Native In-Game Engine Hooks & Campaign Mechanics (`dxgi.dll`)**
- **Dynamic MJOLNIR Shield Capacity Management (Progressive & Glass Cannon Modes)**
- **DeathLink Multiworld Penalties & Dual-Layer Feedback Suppression (`Kill`, `Shield Break`, `Mortal`)**

---

## 1. Test Modalities & Prerequisites

This test plan supports two distinct execution modalities:
* **Mode A: End-User Experience (Release / GUI)**:
  - Intended for players and release-candidate validation.
  - **Zero command-line or Python venv required**.
  - All interactions occur through `ArchipelagoLauncher.exe`, Windows file associations (`.aphalo3`), and client GUI windows.
* **Mode B: Developer / Source Checkout (CLI)**:
  - Intended for development, automated testing, and CI.
  - Uses the Archipelago virtual environment (`.\.venv\Scripts\python.exe`).
  - Commands use `$env:SKIP_REQUIREMENTS_UPDATE="1"` to bypass unrelated world dependency checks.

### Requirements:
1. **Halo: The Master Chief Collection (Steam or PC Game Pass)**:
   - Must have the **Halo 3** DLC installed.
   - Game is launched with **Anti-Cheat Disabled** (`-eac-nop-loaded`) when running Archipelago.
2. **Bundled Native Mod (`dxgi.dll`)**:
   - Bundled at `worlds/halo3/mod/dxgi.dll`.
   - **No manual copying is needed**: The launcher and client automatically copy `dxgi.dll` to `mcc/binaries/win64/` on launch and delete it immediately upon game exit.
   - If modifying C++ source in `mcc_mod/`, compile via `mcc_mod/build.bat`, which updates `worlds/halo3/mod/dxgi.dll` automatically.

---

## 2. Quality Verification & Static Analysis

Before running manual tests, run automated static analysis:

### 2.1 Pyrefly Type Checking
```powershell
.\.venv\Scripts\pyrefly.exe check worlds/halo3/ worlds/halo3/test/ Halo3Client.py --min-severity warn
```
* **Expected Result**: `INFO 0 diagnostics`. Zero errors and zero warnings.

### 2.2 Automated Unit Test Suite
```powershell
$env:SKIP_REQUIREMENTS_UPDATE="1"
.\.venv\Scripts\python.exe -m unittest discover -t . -s worlds/halo3/test -v
```
* **Expected Result**: All 163 tests pass (`OK`).

---

## 3. Step-by-Step Test Cases

---

### Phase 1: Generation, Presets & Patch Containers

#### TC-01: Seed Generation & `.aphalo3` Container Packaging
* **Objective**: Verify that generating a seed creates the `.zip` multiworld archive and a valid `.aphalo3` player container.
* **Steps**:
  1. Ensure a valid player YAML exists at `Players/Halo3Test.yaml`:
     ```yaml
     name: MasterChief
     game: Halo 3
     Halo 3:
       goal: beat_halo
       starting_mission: sierra_117
     ```
  2. Run the generator:
     ```powershell
     $env:SKIP_REQUIREMENTS_UPDATE="1"
     .\.venv\Scripts\python.exe Generate.py --player_files_path Players --seed 12345
     ```
  3. Inspect the created archive in `output/`:
     ```powershell
     cmd /c ".\.venv\Scripts\python.exe -c ""import zipfile; z = zipfile.ZipFile('output/AP_13515657874892102023.zip'); print(z.namelist())"""
     ```
* **Expected Result**:
  - Generator finishes with `Creating final archive... Done.`
  - Inside the `.zip`, there is an `AP_*_P1_MasterChief.aphalo3` file alongside the `.archipelago` and `_Spoiler.txt` files.
  - The `.aphalo3` archive contains `archipelago.json` and `halo3_slot_data.json`.

---

#### TC-02: Option Variations & Item/Location Balancing
* **Objective**: Verify generator mathematically balances item pool and location counts across different settings.
* **Configurations to Test**:
  - **Config A (Full/Maximal)**: `goal: 1`, `weapon_license_mode: 1`, `include_skulls: true`, `include_terminals: true`, `include_rally_points: true` -> **47 locations / 47 items**.
  - **Config B (Tier Progression)**: `goal: 0`, `weapon_license_mode: 2`, `include_skulls: true`, `include_terminals: true`, `include_rally_points: false` -> **29 locations / 29 items**.
  - **Config C (Vanilla Weapons / Minimal Locations)**: `goal: 1`, `weapon_license_mode: 0`, `include_skulls: false`, `include_terminals: false`, `include_rally_points: false` -> **9 locations / 9 items**.
* **Steps**:
  1. Generate each configuration in separate directories:
     ```powershell
     $env:SKIP_REQUIREMENTS_UPDATE="1"
     .\.venv\Scripts\python.exe Generate.py --player_files_path PlayersTest/ConfigA
     ```
* **Expected Result**:
  - All seeds complete with 0 `FillError` exceptions.
  - Location and item counts match exactly for each option combination.

---

#### TC-03: Option Presets & WebHost Validation
* **Objective**: Verify all 5 curated gameplay presets are loaded into `Halo3WebWorld`.
* **Steps**:
  1. Run the presets unit test:
     ```powershell
     $env:SKIP_REQUIREMENTS_UPDATE="1"
     .\.venv\Scripts\python.exe -m unittest worlds.halo3.test.test_basic.TestHalo3Presets -v
     ```
* **Expected Result**:
  - Test confirms presence of `Standard`, `Quick Campaign`, `Full Campaign (All Missions)`, `Tiered Weapons`, and `Vanilla Combat`.
  - On the WebHost game generation page, the presets appear and populate YAML options with one click.

---

### Phase 2: Mod Lifecycle, Launcher & Anti-Cheat Safety

#### TC-04: Offline Client Startup & Pre-Flight Sanitizer
* **Objective**: Confirm the client boots cleanly without MCC running and sanitizes any leftover DLLs from previous sessions.
* **End-User Flow (GUI)**:
  1. Open `ArchipelagoLauncher.exe` and click **Halo 3 Client**.
  2. The Halo 3 Client window appears with a graphical log viewer and a command input box at the bottom.
* **Developer Flow (CLI)**:
  1. Run from the virtual environment:
     ```powershell
     $env:SKIP_REQUIREMENTS_UPDATE="1"
     .\.venv\Scripts\python.exe Halo3Client.py --nogui
     ```
* **Expected Result**:
  - Client logs `Halo 3 MCC Watcher started.`
  - If a stale `dxgi.dll` was present in `mcc/binaries/win64/`, client logs `[Safety] Sanitized stale mod DLL...` and automatically removes it.
  - Client updates `%LOCALAPPDATA%\Halo3AP\state.json`.

---

#### TC-05: Client Utility Commands (`/mod_status`, `/clean_mod`, `/launch`)
* **Objective**: Verify interactive mod management commands in `Halo3Client`.
* **Steps** (Can be typed into either the Client GUI command box or CLI terminal):
  1. Type:
     ```text
     /mod_status
     ```
     - *Expected*: Prints detected MCC path, `MCC Process Running: NO`, `Mod DLL Installed in MCC: NO`, and confirms bundled DLL exists.
  2. Type:
     ```text
     /clean_mod
     ```
     - *Expected*: Outputs `No mod DLL found in MCC directory, or already cleaned.`
  3. Type:
     ```text
     /launch
     ```
     - *Expected*: Automatically deploys `dxgi.dll`, launches MCC with `-eac-nop-loaded`, and starts the cleanup watchdog.

---

#### TC-06: One-Click Launch via Launcher or Client
* **Objective**: Verify automated launch boots MCC with EAC disabled and deploys the mod.
* **End-User Flow (GUI - Recommended)**:
  1. Ensure MCC is closed.
  2. Open `ArchipelagoLauncher.exe`.
  3. Click the **"Halo 3 (Launch Game & Client)"** button.
* **Developer Flow (CLI)**:
  1. Ensure MCC is closed.
  2. Run:
     ```powershell
     $env:SKIP_REQUIREMENTS_UPDATE="1"
     .\.venv\Scripts\python.exe Halo3Client.py --launch
     ```
* **Expected Result**:
  - Client logs: `[Mod Manager] Deployed Archipelago mod DLL: ...\mcc\binaries\win64\dxgi.dll`.
  - Client launches `MCC-Win64-Shipping.exe -eac-nop-loaded`.
  - Client logs: `[Watchdog] Detached cleanup supervisor spawned for MCC PID <pid>`.
  - Halo: MCC opens in Anti-Cheat Disabled mode.
  - `mcc/binaries/win64/dxgi.dll` exists on disk while MCC is active.

---

#### TC-07: Detached Watchdog Process & Guaranteed Cleanup
* **Objective**: Verify that `cleanup.py` automatically deletes `dxgi.dll` when MCC terminates, even if the client crashed or was closed first.
* **End-User Flow**:
  1. Launch MCC using **Halo 3 (Launch Game & Client)**.
  2. Verify MCC is running.
  3. Close the **Halo 3 Client** GUI window completely.
  4. Exit Halo: MCC (select **Quit to Desktop** or press **Alt+F4**).
  5. Check `mcc/binaries/win64/` and `%TEMP%\Halo3AP_cleanup.log`.
* **Developer Flow (CLI)**:
  1. Launch MCC via `Halo3Client.py --launch`.
  2. Force-kill the client terminal process (press **Ctrl+C** or kill process).
  3. Exit Halo: MCC.
* **Expected Result**:
  - Within ~1-2 seconds of MCC closing, `dxgi.dll` is deleted from `mcc/binaries/win64/`.
  - `%TEMP%\Halo3AP_cleanup.log` records:
    ```
    [INFO] Target MCC PID <pid> terminated. Proceeding with cleanup.
    [INFO] Successfully removed dxgi.dll. Game installation is vanilla and safe for EAC.
    ```

---

#### TC-08: In-DLL Anti-Cheat Safety Guard (Normal Mode Passthrough)
* **Objective**: Verify the DLL deactivates completely if MCC is launched normally with EasyAntiCheat enabled (0 ban risk).
* **Steps**:
  1. With MCC closed, copy `worlds/halo3/mod/dxgi.dll` to `mcc/binaries/win64/dxgi.dll`.
  2. Launch MCC via Steam normally (select **"Play Halo: The Master Chief Collection"** with Anti-Cheat Enabled).
  3. Open `%TEMP%\Halo3AP_mod.log`.
* **Expected Result**:
  - Mod's `DllMain` checks `GetCommandLineA()` and finds `-eac-nop-loaded` is absent.
  - Mod logs:
    ```
    [SAFETY] MCC was launched WITHOUT -eac-nop-loaded! Anti-Cheat is active! Mod is entering INERT PASSTHROUGH mode. 0 ban risk.
    ```
  - The DLL installs **zero** hooks, allocates **zero** trampolines, and creates **no** Named Pipes.
  - The game boots normally with no EAC untrusted binary violations.
  - Close MCC and click **Clean Mod** (or run `/clean_mod`) to remove the test DLL.

---

#### TC-09: End-to-End Patch Container Workflow (`.aphalo3`)
* **Objective**: Verify a player can start a game purely through patch container interactions with zero command-line input.
* **End-User Flow (Primary)**:
  1. Generate a seed containing a Halo 3 player.
  2. Locate the generated `AP_*_P1_<Name>.aphalo3` file.
  3. **Method A (File Association)**: Double-click the `.aphalo3` file directly in Windows File Explorer.
  4. **Method B (Archipelago Launcher)**: Open `ArchipelagoLauncher.exe`, click **Open Patch**, and select the `.aphalo3` file.
* **Developer Flow (CLI)**:
  ```powershell
  $env:SKIP_REQUIREMENTS_UPDATE="1"
  .\.venv\Scripts\python.exe Halo3Client.py "output\AP_13515657874892102023_P1_MasterChief.aphalo3"
  ```
* **Expected Result**:
  - Archipelago matches `.aphalo3` to `Halo 3 Client`.
  - Client boots, unpacks `archipelago.json` and `halo3_slot_data.json`.
  - Client automatically populates the server address and player name.
  - Client automatically checks for MCC and deploys `dxgi.dll`.

---

### Phase 3: Network Connection & IPC Synchronization

#### TC-10: MultiServer Hosting & Client Connection
* **Objective**: Verify full network handshake between client and server.
* **End-User Flow (GUI)**:
  1. In the Halo 3 Client window, verify Server field is set (e.g. `archipelago.gg:38281` or `localhost:38281`).
  2. Click **Connect** (or press Enter).
* **Developer Flow (CLI)**:
  1. Start MultiServer in Terminal 1:
     ```powershell
     $env:SKIP_REQUIREMENTS_UPDATE="1"
     .\.venv\Scripts\python.exe MultiServer.py output/AP_13515657874892102023.zip
     ```
  2. Start Client in Terminal 2:
     ```powershell
     $env:SKIP_REQUIREMENTS_UPDATE="1"
     .\.venv\Scripts\python.exe Halo3Client.py --connect localhost:38281 --name MasterChief
     ```
* **Expected Result**:
  - Server accepts connection and sends slot data.
  - Client receives starting mission unlock: `>> UNLOCKED MISSION: Sierra 117! <<`.
  - Client receives starting weapon licenses (`Assault Rifle License` and `Plasma Pistol License`).

---

#### TC-11: Named Pipe IPC & State Sync
* **Objective**: Verify client pushes state to native mod over `\\.\pipe\Halo3ArchipelagoIPC`.
* **Steps**:
  1. Have MCC running with mod loaded.
  2. Connect client to MultiServer.
  3. In Terminal 3, monitor mod log:
     ```powershell
     Get-Content "$env:TEMP\Halo3AP_mod.log" -Wait -Tail 20
     ```
* **Expected Result**:
  - Client logs `[Pipe] Connected to native mod Named Pipe!`.
  - Mod logs:
    ```
    [PIPE] Received state update: unlocked_missions=[Sierra 117], unlocked_weapons=[assault_rifle, plasma_pistol]
    ```

---

### Phase 4: In-Game Progression & Enforcement

#### TC-12: Mission Gating (Native Mod Access Denied)
* **Objective**: Confirm locked campaign missions cannot be launched.
* **Steps**:
  1. In the MCC menu, navigate to **Halo 3** -> **Campaign** -> **Missions**.
  2. Select a locked mission (e.g. **Crow's Nest**).
  3. Select difficulty and click **Start Game**.
* **Expected Result**:
  - Match countdown does **not** start.
  - Game cleanly remains in the Campaign menu.
  - Mod log outputs:
    ```
    [AP BOUNCER] ACCESS DENIED! Mission 'Crow's Nest' (020_base) is LOCKED in Archipelago!
    [AP BOUNCER] Cleanly blocking StartCountdown! Countdown will NOT start.
    ```

---

#### TC-13: Real-Time Mission Unlocking
* **Objective**: Verify that receiving a mission item immediately allows starting the mission without restarting MCC.
* **Steps**:
  1. Send the item:
     - **Option A (Via Halo 3 Client `/complete`)**: Run `/complete` for the check that contains the item (consult spoiler log). For example:
       ```text
       /complete The Covenant
       ```
     - **Option B (Via Server Console)**: In the MultiServer terminal window:
       ```text
       send MasterChief Crow's Nest Unlock
       ```
     - **Option C (Client `!getitem`)**: If `item_cheat: 1` or `2` is enabled in `host.yaml`:
       ```text
       !getitem Crow's Nest Unlock
       ```
  2. Observe client and mod logs.
  3. In MCC, select **Crow's Nest** and click **Start Game**.
* **Expected Result**:
  - Client logs: `>> UNLOCKED MISSION: Crow's Nest! <<`.
  - Mod log: `[PIPE] Received state update: unlocked_missions includes Crow's Nest`.
  - Countdown begins and Crow's Nest loads successfully.

---

#### TC-14: Starting Weapon Spawning & Sidearm Guarantee
* **Objective**: Verify Master Chief spawns with his authorized starting weapons upon loading a mission.
* **Steps**:
  1. Launch Sierra 117.
  2. Observe Chief's equipped weapons upon gaining control.
* **Expected Result**:
  - Chief holds his configured starting weapon (Assault Rifle) and secondary energy sidearm (Plasma Pistol).
  - Mod log confirms:
    ```
    [WEAPON] Assigned starting weapon: assault_rifle and plasma_pistol
    ```

---

#### TC-15: Weapon License Enforcement (Equipped Weapon Strip)
* **Objective**: Verify unauthorized weapons in Chief's loadout are stripped.
* **Steps**:
  1. Start Sierra 117.
  2. Attempt to equip or hold an unauthorized weapon (e.g. Battle Rifle if license not owned).
* **Expected Result**:
  - Mod detects weapon tag without corresponding license.
  - Chief drops or loses unauthorized weapon.
  - Mod log:
    ```
    [WEAPON] Unauthorized weapon detected (battle_rifle)! Stripping from player loadout.
    ```

---

#### TC-16: Weapon Pickup Interception
* **Objective**: Verify Chief cannot pick up unauthorized weapons from the ground or weapon racks.
* **Steps**:
  1. Approach an unauthorized weapon on the ground (e.g. Sniper Rifle or Brute Shot).
  2. Press and hold the interact/swap button (`E` on keyboard, `RB`/`X` on controller).
* **Expected Result**:
  - Weapon is **not** picked up.
  - Mod log outputs:
    ```
    [WEAPON] Intercepted pickup: sniper_rifle license NOT owned. Denying pickup action.
    ```

---

#### TC-17: Real-Time Weapon License Receiving & Tier Mode
* **Objective**: Verify receiving a weapon license instantly unlocks pickup and equip capabilities in real time.
* **Steps**:
  1. While standing next to a locked weapon (e.g. Sniper Rifle), grant the license (type into the **Halo 3 Client** command bar or MultiServer console):
     ```text
     !getitem Sniper Rifle License MasterChief
     ```
  2. Attempt to pick up the Sniper Rifle again.
* **Expected Result**:
  - Client updates mod in < 500ms: `[Pipe] Pushed state update`.
  - Chief successfully picks up and equips the Sniper Rifle.

---

### Phase 5: Event Detection, Auto-Return & Stability

#### TC-18: Mission Completion Auto-Detection & Clean Menu Return
* **Objective**: Verify `game_won` hook fires at the end of a mission, skips auto-advancing to the next level, returns cleanly to the menu, and sends the completion check.
* **Steps**:
  1. Launch Sierra 117.
  2. Play to the final Pelican extraction outro cutscene.
  3. Watch the game behavior as the cutscene concludes.
* **Expected Result**:
  - Native mod hooks `game_won` (`halo3.dll + 0x1F2F7C`).
  - Outro cutscene ends; MCC cleanly returns to Campaign Lobby without loading Crow's Nest and without hanging on a black screen.
  - `Halo3Client.py` logs:
    ```
    [Game Won] Mission complete: Sierra 117
    [Game Won] Sending location check: Sierra 117 - Mission Complete
    ```
  - MultiServer registers check `117101` and releases rewards.

---

#### TC-19: In-Game Event Checks (Skulls, Terminals & Rally Points)
* **Objective**: Verify collecting campaign collectibles fires native hooks and sends checks instantly.
* **Steps**:
  1. Start Sierra 117 on any difficulty (including Easy) and from any rally point (vanilla Easy difficulty and mid-mission insertion point restrictions bypassed via native mod patches).
  2. Cross the checkpoint milestone to trigger Rally Point Bravo.
  3. Pick up the **Blind Skull** near the cliff.
  4. In The Ark or Crow's Nest, interact with a Forerunner terminal.
* **Expected Result**:
  - Mod log records events:
    - `[EVENT] Rally point reached! rallyIndex=1`
    - `[EVENT] Secondary skull collected! skullIndex=0`
    - `[EVENT] Terminal accessed! index=0`
  - `Halo3Client.py` logs:
    - `[Rally] Reached Sierra 117 - Rally Point Bravo!`
    - `[Skull] Collected Sierra 117 - Blind Skull!`
    - `[Terminal] Accessed Terminal 1!`
  - Checks are immediately forwarded to MultiServer.

---

#### TC-20: Clean Process Exit & Alt+F4 Stability
* **Objective**: Verify that quitting MCC or pressing Alt+F4 during a mission does not trigger `0xC0000005` access violations or LoaderLock deadlocks.
* **Steps**:
  1. While playing in an active mission with `halo3.dll` loaded, press **Alt+F4** or select **Quit to Desktop**.
* **Expected Result**:
  - Game terminates instantly with zero error dialogs.
  - `dxgi.dll`'s `DllMain` safely bypasses teardown when `lpReserved != nullptr`.
  - Task Manager confirms `MCC-Win64-Shipping.exe` does not hang.
  - Detached watchdog immediately deletes `dxgi.dll`.

---

#### TC-21: Manual CLI Commands (`/status` & `/complete`)
* **Objective**: Verify manual fallback commands in `Halo3Client.py`.
* **Steps**:
  1. In client terminal, run `/status`.
     - *Expected*: Displays MCC connection status, active map, all 9 missions with lock/completion status, and weapon licenses.
  2. Run `/complete Sierra 117 - Blind Skull`.
     - *Expected*: Sends location check `117201` and confirms completion.
  3. Run `/complete Sierra 117 - Blind Skull` a second time.
     - *Expected*: Outputs `'Sierra 117 - Blind Skull' is already completed.` without resending network traffic.

---

#### TC-22: Campaign Submenu Direct-Skip, Mission Carousel Filtering & Multi-Game Stability
* **Objective**: Verify that in-flight VFS PAK patches (`DT_MainMenuData`) skip intermediate submenus directly to Mission Select when selecting Halo 3, native Scaleform carousel interception (`UIWidgetArray::AddItem` at `+0x895F54`) dynamically omits locked mission cards, and multi-game campaign navigation remains stable.
* **Steps**:
  1. Launch MCC with native mod active and connect the client to a seed where only Sierra 117 is unlocked.
  2. In the main menu, navigate to **Campaigns** -> **Halo 3**.
  3. Notice that intermediate submenus (such as Playlists, Quickstart, or Built-In vs Mods) are automatically bypassed via in-flight PAK byte patching in `vfs_hooks.cpp` (`DirectMissions` and `FeatureID`), navigating straight to the Mission Select screen.
  4. Observe the mission cards shown in the horizontal carousel: only unlocked missions (e.g. Sierra 117) are displayed; locked missions are dynamically dropped from the carousel array by `UIWidgetArray::AddItem`.
  5. Unlock a new mission (e.g. `Crow's Nest`) via `/complete` or client cheat, re-open the mission carousel, and observe the new card seamlessly populate.
  6. Return to the main menu, select **Campaign**, and click on another game (e.g. **Halo 2** or **Halo: Reach**) to verify vanilla campaigns load cleanly without crashing or freezing.
* **Expected Result**:
  - Selecting Halo 3 immediately opens the Mission Select carousel, skipping intermediate submenus.
  - In step 4, only unlocked mission cards appear in the carousel.
  - In step 5, newly unlocked mission cards populate dynamically.
  - In step 6, clicking other campaigns passes cleanly through to vanilla MCC without freezing or crashing.
  - Mod logs confirm:
    ```
    [VFS] >>> ReadFileSync: Reading DT_MainMenuData range!
    [MissionFilter] AddItem hook active at ...
    [MissionFilter] >>> DROPPING LOCKED MISSION CARD: idx=2 (020_base) <<<
    ```

---

#### TC-23: Dynamic Pattern Scanning & Fallback Verification
* **Objective**: Verify `%TEMP%\Halo3AP_mod.log` records dynamic pattern scan resolution for engine hooks and verified RVAs for `halo3.dll`.
* **Steps**:
  1. Check `%TEMP%\Halo3AP_mod.log` or debug output on game boot and during map loading.
* **Expected Result**:
  - Log records dynamic pattern resolution for Scaleform carousel interception:
    - `[MissionFilter] AddItem hook active at ...` (or fallback `AddItem pattern not found, using RVA +0x895F54`)
  - Lobby hooks (`Detour_CommitSelection`, `Detour_InitializeLevel`, `Detour_StartCountdown`) install cleanly.
  - Engine hooks in `g_halo3Hooks` (`weapon_evaluate_candidate`, `can_trade_with_ally`, `Halo3EngineStep`, etc.) resolve via verified static RVAs or dynamic pattern scanning, confirmed by `[HOOK] Installed detour for ...`.

---

#### TC-24: Engine-Level ShellCommand Return-to-Menu
* **Objective**: Verify mission win uses `IGameEngine::ShellCommand` to teardown the session without relying on fragile branch patches.
* **Steps**:
  1. Finish any campaign mission (or trigger `game_won`).
  2. Monitor `%TEMP%\Halo3AP_mod.log`.
* **Expected Result**:
  - Log records:
    - `[EVENT] game_won called! Mission complete!`
    - `[ShellCommand] Intercepting command 0xD - sending clean teardown to menu...`
  - Game transitions cleanly back to the Campaign mission select lobby without advancing to the next level in sequence.

---

#### TC-25: In-Game DeathLink Execution
* **Objective**: Verify that incoming DeathLinks trigger the configured in-game penalty (`Kill`, `Shield Break`, `Mortal`).
* **Steps**:
  1. Launch MCC with native mod and connect client.
  2. Start any campaign mission (e.g. Sierra 117) and wait for Master Chief to spawn with weapons equipped past the 180-tick grace period.
  3. In client console, run `/deathlink test kill`. Observe Chief die and checkpoint reload without crashing or freezing.
  4. Run `/deathlink test shield_break`. Observe shields instantly drop to 0% with EMP spark sound and shield bar fully depleting.
  5. Run `/deathlink test mortal`. Observe shields drop to 0% and health bar drop to 10% red (one hit from death).
* **Expected Result**:
  - Each penalty executes smoothly during active gameplay.
  - Mod logs penalty execution: `[DeathLink] EXECUTING <KILL/SHIELD BREAK/MORTAL DAMAGE>...`.

---

#### TC-26: Feedback Loop Suppression & Amnesty
* **Objective**: Verify incoming kills do not echo back outward, and amnesty absorbs deaths.
* **Steps**:
  1. Generate or configure a YAML profile with `death_link: true` and `death_link_amnesty: 2`.
  2. Connect to the multiworld room.
  3. Die naturally in-game twice.
  4. Die a third time naturally in-game.
  5. Receive an incoming DeathLink kill (or run `/deathlink test kill`).
* **Expected Result**:
  - On first two deaths, client log shows: `[DeathLink] Local death absorbed by amnesty (1/2)` and `(2/2)`. No DeathLink is broadcast to multiworld.
  - On third death, client broadcasts `DeathLink` to the multiworld room: `[DeathLink] Amnesty exhausted! Sending death to multiworld: ...`.
  - When dying due to an incoming DeathLink kill, neither the native mod nor the client re-broadcasts the death outward (no feedback loop). Mod log confirms `s_wasChiefAlive = false` debounce and client suppresses with `is_penalty_death_pending`.

