# Halo 3 / MCC Reverse Engineering & Hook Architecture

This document details the reverse-engineering discoveries, memory layouts, hook points, and failed attempts encountered while developing the native Archipelago mod for *Halo: The Master Chief Collection* (Halo 3 engine).

All memory addresses and struct offsets are declared in:
- C++: `mcc_mod/offsets.h`
- Python: `worlds/halo3/constants.py`

---

## 1. Failed Hook Points & Postmortem (What Did NOT Work)

During research and development, several candidate hook points and gating strategies were tested and failed for subtle engine architectural reasons. These are documented below to prevent future regressions.

### 1.1 MCC Process & Campaign Lobby (UI & Level Gating)

#### ❌ `MCC + 0x512B90` — `CampaignLobby::ActionDispatcher`
* **Signature**: `bool __fastcall (void* lobby, uint32_t actionId, void* r8)`
* **Intended Role**: Intercept the central UI action handler when "Start Game" was clicked.
* **Why it failed**: MinHook places a 5-byte `JMP` trampoline in the function prologue. In MCC, the Scaleform / Unreal Engine UI framework uses stack walking and register state to perform mouse hit-testing and input dispatching. Detouring `+0x512B90` caused Windows to drop mouse clicks throughout the campaign menus (clicking missions, difficulty, or options stopped responding), even though arrow keys and gamepad navigation continued to work.

#### ❌ `MCC + 0x50E1E4` — `CampaignLobby::CommitSelectionOrCountdown`
* **Intended Role**: Block countdown initiation by intercepting selection commits.
* **Why it failed**: This function is shared across multiple menu operations. In particular, it is called to commit list selection changes when the user clicks a different mission in the campaign missions list. Detouring it and returning early prevented new mission selections from registering, locking the player onto whatever level was previously selected (e.g. Crow's Nest).

#### ❌ `MCC + 0x512F25` & `+0x512FCC` — `CampaignLobby::CountdownUIAnimationTrigger`
* **Intended Role**: Bypass the countdown timer by detouring the timer animation trigger.
* **Why it failed**: These instructions only control the Scaleform UI countdown animation widget and submenu transition. The underlying game engine countdown timer continued running asynchronously in the background (resulting in hearing the countdown beeps without seeing the UI), and detouring these bytes also broke mouse clicks on difficulty and skulls.

#### ❌ `MCC + 0x1E6D80` — `Engine::LaunchMission`
* **Intended Role**: Intercept mission launch at the core engine level.
* **Why it failed**: Disassembly at `+0x1EC3B7` revealed that MCC invokes this function and completely ignores its return value. Returning `0` or `false` did not halt the engine; it proceeded to load the map regardless.

#### ❌ `MCC + 0x512E9B` — `Shared Advance Handler`
* **Intended Role**: Intercept advancing from submenu to game launch.
* **Why it failed**: This function was actually the menu item activation/advance handler. Returning null or jumping to `0x512FA2` caused clicking menu items to advance to the next item down the list, and `0x512FA2` then jumped straight to `0x512F62` (launching the mission immediately).

#### ❌ MCC Campaign Menu Modification — Scaleform Flash Architecture (Multiple Attempts)
* **Goal**: Simplify the campaign menu flow from `Campaigns → Game Select → Content Type → RESUME/QUICKSTART/MISSIONS/PLAYLISTS` to `Campaigns → Halo 3 → BUILT IN → MISSIONS` by hiding or removing unwanted menu entries.
* **Investigation summary**: Five rounds of hook attempts targeting different functions, all of which installed successfully via MinHook but were **never called** during normal menu navigation:
  1. `+0x4E8294` — Campaign submenu populator (references `root.Container.mcArray.array`). Never called during menu navigation.
  2. `+0x4BB2E8` — Mission list populator (references `root.mcScreen.mcContainer.mcArray.array`, `populateArray`, `root.setupScreen`). Never called.
  3. `+0x4CE4AC` / `+0x4DDDB8` — ShowGameType functions (reference `root.showGameType`, `root.setDefaultPosition`). Never called.
  4. `+0x711E30` — Scaleform `GFx::Movie::SetVariable` implementation (references `"SetVariable failed: can't resolve the path \"%s\""`). Never called through this path.
  5. `+0x35A7E4` / `+0x44C920` — `populateArray` callers. Never called.
  6. `+0x513578` — CampaignLobby Menu Selection sub-handler. Never called (only active within the final campaign lobby screen).
  7. `+0x36F130` — UI navigation handler (references `root.accept`, `root.cancel`, `root.scrollRight`, `root.scrollLeft`). Never called.
  8. `+0x4E0068` — Compact menu handler (immediately checks `0xF3A4CF96` hash). Never called.
* **Root cause — Flash/ActionScript-driven menus**:
  - MCC's campaign menus are pre-compiled SWF Flash movies running inside the **Scaleform GFx VM**.
  - The C++ code populates data (game lists, mission lists) into Scaleform arrays **once** during movie initialization (at MCC startup or lobby creation), then the Flash ActionScript VM handles ALL menu navigation transitions internally.
  - Menu clicks (game select, content type, MISSIONS/QUICKSTART/PLAYLISTS) are **Flash-to-Flash state transitions** — the C++ layer is never invoked for these transitions.
  - The C++ `ActionDispatcher` (`+0x512B90`) only receives callbacks from Flash at the **final boundary points** (e.g., "Start Game" clicked → `StartCountdown` at `+0x5137EC`), not during intermediate navigation.
* **Key discovered Scaleform paths and strings**:
  - `root.Container.mcArrayGameSelect.Array` — Game selection list (Reach, CE, H2, H3, ODST, H4)
  - `root.Container.mcArrayInstallation.Array` — Content/installation list (BUILT IN, DLC)
  - `root.Container.mcArray.array` — Campaign submenu list (RESUME, QUICKSTART, MISSIONS, PLAYLISTS)
  - `root.mcScreen.mcContainer.mcArray.array` — Mission list within campaign lobby
  - `root.changeHeaderText` — Sets the header text (e.g., game title)
  - `root.setMissionInfo` — Sets mission description
  - `root.showGameType` — Shows the content type selection screen
  - `root.showDifficulty` — Shows difficulty selection
  - `root.setDefaultPosition` — Resets UI positioning
  - `root.accept` / `root.cancel` — Accept/back navigation actions
  - `populateArray` — Generic Scaleform array population call
  - `populateContentDifficulty` — Difficulty array population
  - `campaignlobbytab` — Campaign lobby tab identifier
* **Known action hashes used in menu dispatch** (CampaignLobby `+0x512B90`):
  - `0xF3A4CF96` — Menu Selection (entering submenus)
  - `0xAC6F1146` — Back / Cancel
  - `0xAE68D666` — Direct "Start Game" from main lobby
  - `0xCDF8C30A` — "Start Game" from submenu
  - `0x044563CE` — Checkpoint Overwrite Confirmation
  - `0xF221ECB1` — Frame Tick (~60 Hz)
  - `0x687F2B16` — Mouse Hover / Motion
* **Potential future approaches** (not yet attempted):
  1. **Scaleform movie creation interception**: Hook `GFx::Loader::CreateMovie` or the movie initialization path to get a `GFx::Movie*` pointer, then call `Invoke("removeMovieClip")` or modify array elements through the vtable after population completes.
  2. **SWF file modification**: Locate and modify the campaign lobby SWF files in MCC's data packages to remove unwanted UI elements at the Flash level.
  3. **Input simulation**: Programmatically navigate past unwanted menus by sending synthetic keyboard/controller inputs to auto-select "Halo 3" → "BUILT IN" → "MISSIONS".
  4. **ExternalInterface callback hooking**: Scaleform Flash movies communicate with C++ through ExternalInterface callbacks. Hooking the ExternalInterface registration or dispatch could provide visibility into Flash→C++ communication.
* **Important note**: Mouse input is NOT affected by MinHook detouring at these addresses (confirmed safe across all tests). The mouse-breaking issue is specific to `+0x512B90` (ActionDispatcher) and `+0x50E1E4` (CommitSelectionOrCountdown) which participate in the Scaleform event loop.

---

### 1.2 Halo 3 Engine (`halo3.dll`) Gameplay & Weapon Gating

#### ❌ `halo3.dll + 0x8D258` — `weapon_can_interact`
* **Intended Role**: Evaluate weapon interactability when looking at weapons on the ground.
* **Why it failed**: This function is an internal weapon state query that is not part of the active player interaction candidate pipeline. Detouring it had zero effect on in-game weapon pickup prompts or swap interactions.

#### ❌ `halo3.dll + 0x5A2A30` & `+0x3B2740` — Action Table Callbacks
* **Intended Role**: Suppress HUD action prompts ("Hold [E] to swap / pick up" and "Hold [E] to trade") for locked weapons and allies.
* **Why they failed**:
  - `+0x5A2A30` is an internal spatial navigation pathfinding query taking an 8-bit index in `%edx`, completely unrelated to candidate evaluation (0 hits in logs).
  - `+0x3B2740` is an action table callback registered in the interaction dispatch table at `halo3.dll + 0x8B3268`. During normal aiming and gameplay, this function was **never called** (0 hits in logs). Detouring it did not suppress HUD prompts or prevent interaction prompts from rendering.
* **The Solution**: Reverse-engineering the CHUD datasource (`c_player_select_actions_datasource` with vtable at `+0x80F8A0`) and tracing candidate relevance formatting strings (`weapon-pickup : relevance=%5.3f` at RVA `0x91C67`) led directly to the real candidate qualification functions in the interaction vtables:
  - **Ground Weapons**: `halo3.dll + 0x91694` (`weapon_evaluate_candidate`, Method 2 in `weapon-pickup` vtable `0x180768358`).
  - **Ally Unit Trading**: `halo3.dll + 0x8B52C` (`unit_evaluate_candidate`, Method 2 in `unit-pickup` vtable `0x180766F78`).
  - Returning `false` (0) from these functions immediately rejects the candidate from the player's interactable candidate list: the HUD crosshair prompt never appears, proximity auto-pickup never activates, and reload is 100% uninterrupted.

#### ❌ Dual-Wield Proximity Conflicts & Hold-to-Swap Reset (The Needler Swap Bug)
* **Intended Role**: Allow player to hold the action button to swap an active weapon for an unlocked weapon on the ground (e.g. holding AR + Spiker and swapping for a Needler).
* **Why it failed**:
  - Spiker is a one-handed / dual-wieldable weapon (`can_dual_wield == 1`). Needler in Halo 3 is a two-handed weapon (`can_dual_wield == 0`).
  - When Chief holds an AR (slot 0) and Spiker (slot 1) and stands over a Needler, Chief's inventory is full. The engine's proximity auto-pickup action (`+0x3B2F18`) attempts to evaluate the Needler as a dual-wield candidate (`slotPref=3`).
  - In `unit_add_weapon` (`+0x358024`), line `0x358133` tests `can_dual_wield`. Because the Needler cannot be dual-wielded, it fails and returns `0` every simulation frame (~60 Hz).
  - Each time the action fails, the engine calls `0x3B2BC8`, which zeroes out the interaction state (`[rdi+0x6] = 0`). This continuously wiped the player's hold timer (`[rdi+0x8]`) before it could reach the required hold threshold (`[rdi+0x9]`), completely preventing the hold-to-swap action (`+0x3B233C`) from ever executing!
* **The Solution**: Filtering candidates at the qualification level (`+0x91694`) and handling auto-pickup without resetting manual hold timers allows the circular hold meter to complete cleanly and execute the swap.

#### ❌ Blocking Solely at `unit_add_weapon` (`+0x358024`) (The Auto-Pickup Reload Loop)
* **Intended Role**: Use `unit_add_weapon` as the sole choke point for weapon gating.
* **Why it failed**: When Master Chief is holding only one weapon (secondary slot empty), the engine runs a proximity auto-pickup check every simulation tick (~60 Hz) whenever Chief is standing near a dropped weapon. If `unit_add_weapon` simply blocks the weapon, the auto-pickup action never completes and retries every 16ms. This repeated state interruption constantly cancelled any active weapon reload animation, making it impossible to reload when standing over a dropped locked weapon. The fix required detouring `weapon_evaluate_candidate` at `halo3.dll + 0x91694` so locked weapons are never considered pickup candidates in the first place.

#### ❌ Unconditional Weapon Stripping (The Unarmed Glitch)
* **Intended Role**: Strip all unauthorized weapons on spawn or level start in `CheckAndEnforcePlayerInventory`.
* **Why it failed**: If the player starts a mission where all initial spawn weapons are locked (e.g. starting on The Ark with a Sniper Rifle when Sniper is locked), stripping all inventory weapons leaves Chief completely unarmed (`totalWeapons == 0`). In Halo 3, Chief has no unarmed combat animations; being completely unarmed breaks weapon switching, animations, and can trigger engine assertions. The fix was implementing the **"Last Weapon Safety Net"**: Chief is never stripped of his last remaining weapon until an authorized weapon is acquired, and `unit_add_weapon` allows an unauthorized starter weapon as an emergency fallback if Chief is completely unarmed.

#### ❌ Thread-Local Storage (`gs:[0x58]`) Resolution Outside the Simulation Thread
* **Intended Role**: Read `TLS_INDEX` and dereference `gs:[0x58]` from external threads or render hooks to inspect objects.
* **Why it failed**: The Blam engine's Thread-Local Storage slots are only allocated and populated on the dedicated Halo 3 simulation thread. Calling `ReadGs58()` from arbitrary threads produces null or invalid pointers. The fix was anchoring all inventory checks to `Halo3EngineStep` (`+0xB2538`) which executes directly on the simulation thread, and caching the master object table pointer (`threadData + 0x38 -> +0x48`) for safe cross-thread reads.

---

## 2. Winning Hook Architecture (What Works)

### 🎯 MCC Gating: `CampaignLobby::StartCountdown` (`MCC + 0x5137EC`)
* **Signature**: `void __fastcall (void* lobby, uint64_t mode)`
* **Function Prologue**:
  ```asm
  mov [rsp+8], rbx     ; 48 89 5C 24 08
  mov [rsp+0x10], rsi  ; 48 89 74 24 10
  push rdi             ; 57
  sub rsp, 0x30        ; 48 83 EC 30
  ```
* **Why this is the optimal hook point**:
  - It is the **sole convergence point** for all three match launch routes:
    1. `0x512663` (Direct "Start Game" clicked from main campaign lobby)
    2. `0x512F6C` ("Start Game" clicked from rally point / settings submenu)
    3. `0x51317B` ("Continue" clicked on checkpoint overwrite confirmation dialog)
  - It is **never** called during:
    - Frame ticks (`0xF221ECB1`)
    - Mouse movement or hover (`0x687F2B16`)
    - Menu navigation (browsing levels, rally points, skulls, difficulty, options)
  - Returning early from this hook cleanly cancels the countdown before any engine transition begins, keeping the player smoothly inside the menu with 100% responsive mouse and keyboard controls.

---

## 3. Campaign Lobby Memory Layout

### Lobby Struct Member Offsets (relative to `lobby` pointer)
| Offset | Type / Description |
|---|---|
| `+0x000` | Vtable pointer (points into `MCC-Win64-Shipping.exe .rdata`) |
| `+0x058` | Object type identifier string (`"CampaignLobby"`) |
| `+0x0B8` | Countdown state integer (`-1` = running, `0`/`1` = idle) |
| `+0x1E8` | Header string pointer (e.g. `"$CAMPAIGN_H3_HEADER"`) |
| `+0x220` | Selected chapter token pointer (e.g. `"$H3_Chapter_3-1_Title"`) |
| `+0x240` | Secondary UI component controller |
| `+0x298` | Start button controller pointer |
| `+0x910` | Primary lobby UI widget controller |

### Campaign Lobby Action Hashes
| Hash (Hex) | Action |
|---|---|
| `0xF221ECB1` | Frame Tick (per-frame lobby update, 60+ Hz) |
| `0x687F2B16` | Mouse Hover / Motion |
| `0xF3A4CF96` | Menu Selection (entering submenus) |
| `0xAC6F1146` | Back / Cancel (Escape / Gamepad B) |
| `0xAE68D666` | Direct "Start Game" clicked from Main Lobby |
| `0xCDF8C30A` | "Start Game" clicked from Submenu |
| `0x044563CE` | Checkpoint Overwrite Confirmation Dialog |

---

## 4. Halo 3 Engine (`halo3.dll`) Interaction & Loadout Architecture

When a map loads, MCC maps `halo3.dll` into the process address space. A background monitor thread detects when `halo3.dll` is loaded and installs the following hooks:

### 1. Interaction Candidate Add Hook (`halo3.dll + 0x38DB2C`)
* **Signature**:
  ```cpp
  bool __fastcall UnitInteractionCandidateAdd(
      uint32_t unitDatum,
      uint32_t actionId,
      uint32_t targetDatum,
      float relevance,
      uint32_t a5,
      uint32_t a6
  );
  ```
* **Role**: The universal bottleneck where ALL crosshairs, raycasts, spheres, and proximity checks submit candidate interactable entities (ground weapons, ally trades, vehicles, devices) to the player's candidate list.
* **Logic**:
  - `actionId == 0x27`: Ground weapon pickup / swap candidate. `targetDatum` is the dropped weapon entity datum. If the weapon is locked in Archipelago, returns `false` (`0`).
  - `actionId == 0x24 || actionId == 0x6`: Ally weapon trade candidate. `targetDatum` is the ally unit datum (e.g. Marine). Resolves ally held weapon at `allyObj + 0x268 + activeSlot * 4`. If the ally's held weapon is locked in Archipelago, returns `false` (`0`).
* **Effect**:
  - The locked entity is never added to Master Chief's interaction state.
  - Crosshair HUD prompts ("Hold [E] to swap / pick up" or "Hold [E] to trade") NEVER appear.
  - Action button hold/press never starts a swap timer for locked items, allowing player reloads to proceed 100% uninterrupted.

### 2. Unit Update Weapon Prompt Hook (`halo3.dll + 0x3B59EC`)
* **Signature**: `bool __fastcall UnitUpdateWeaponPrompt(uint32_t unitDatum, void* candidateData)`
* **Role**: Registered in the interaction dispatch table at `.data + 0x8B3588`. Called every simulation frame when Chief looks at a ground weapon to format and render the "Hold [E] to swap / pick up" prompt.
* **Logic**: Reads weapon datum at `candidateData + 0x8`. If the ground weapon is locked in Archipelago, returns `false` (`0`), cleanly suppressing the weapon prompt from ever being built or rendered.

### 3. Unit Update Trade Prompt Hook (`halo3.dll + 0x3B6398`)
* **Signature**: `bool __fastcall UnitUpdateTradePrompt(uint32_t unitDatum, void* candidateData)`
* **Role**: Registered in the interaction dispatch table at `.data + 0x8B3600`. Called every frame when Chief aims at an ally unit to format and render the "Hold [E] to trade" prompt.
* **Logic**: Uses native candidate resolver `unit_find_interaction_candidate` (`+0x38DC80`) with `actionId == 6` (ally trade) to resolve the target ally. Inspects the ally's currently held weapon. If the weapon is locked in Archipelago, returns `false` (`0`), cleanly suppressing "Hold [E] to trade" from rendering.

### 4. CHUD Set Interaction Prompt Hook (`halo3.dll + 0x382D88`)
* **Signature**: `uint32_t __fastcall ChudSetInteractionPrompt(uint32_t unitDatum, void* promptData)`
* **Role**: The UI rendering boundary that displays HUD action prompts ("Hold [E] to swap for...", "Hold [E] to trade with...").
* **Logic**: Intercepts `promptData + 0x8` (action type `0x27` for weapon, `0x24` for trade). Correlates with candidate table (where trade has `act == 6`). If locked, immediately returns `0` without invoking the engine's CHUD rendering method.

### 5. Unit Action Swap Weapon Hook (`halo3.dll + 0x3B2DAC`)
* **Signature**: `bool __fastcall UnitActionSwapWeapon(uint32_t unitDatum, void* interactionData)`
* **Role**: Manual weapon swap handler called when the player completes holding the action button.
* **Mechanism**: Halo 3 does not have an atomic "swap" function. Swapping drops the held weapon via `0x359C7C` (`unit_drop_weapon_in_slot`), creating an empty inventory slot, which is then populated by proximity auto-pickup.

### 6. Unit Action Pickup Hook (`halo3.dll + 0x3B2F18`)
* **Signature**: `bool __fastcall UnitActionPickup(uint32_t unitDatum, void* interactionData)`
* **Role**: Proximity auto-pickup action evaluated when the player has an empty weapon slot.
* **Logic**:
  - If the target weapon is locked, returns `false` (`0`), cleanly aborting proximity auto-pickup without cycling failure loops or interrupting reload.
  - **Dual-Wield Conflict Fix (The Needler Swap Bug)**: If `slotPref == 3` (dual-wield hand) and the target weapon is not dual-wieldable (`!IsWeaponDualWieldableByName`), returns `false` immediately. This prevents `unit_add_weapon` from failing every frame and invoking `0x3B2BC8`, which would wipe Chief's button hold timer and prevent swapping.

### 7. Unit Add Weapon Hook (`halo3.dll + 0x358024`)
* **Signature**: `bool __fastcall UnitAddWeapon(uint32_t unitDatum, uint32_t weaponDatum, int slotPref)`
* **Role**: Final safety net called when scripts, cutscenes, trades, or spawns equip weapons to characters.
* **Logic**: If the target unit is Master Chief and the weapon is locked, the call is blocked (while preserving the unarmed safety net).

### 8. Engine Simulation Step Hook (`halo3.dll + 0xB2538`)
* **Signature**: `void __fastcall Halo3EngineStep()`
* **Role**: Executes per simulation frame on the dedicated Halo 3 simulation thread where Thread-Local Storage (`gs:[0x58]`) is populated.
* **Logic**:
  - Triggers the dynamic tag table scan on first simulation tick of each new level.
  - Calls `CheckAndEnforcePlayerInventory()` to strip unauthorized weapons and guarantee the unarmed safety net.

### 9. Rally Point Unlock Hook (`halo3.dll + 0xF0810`)
* **Signature**: `void __fastcall GameInsertionPointUnlock(int32_t rallyIndex)`
* **Role**: Engine routine invoked when Chief crosses into a new insertion zone.
* **Logic**:
  - `rallyIndex` arrives in `ecx` (`1` = Bravo, `2` = Charlie).
  - Emits `rally_1:<scenario>` or `rally_2:<scenario>` to `pendingEvents`, appending the active scenario code from MCC's scenario struct for atomic attribution.

### 10. Terminal Activation Hook (`halo3.dll + 0x3A8798`)
* **Signature**: `void __fastcall Detour_TerminalActivate(uint32_t deviceDatum)`
* **Role**: The true native engine function invoked when the player activates/interacts with any terminal device (e.g. holds [E] / RB).
* **Logic**:
  - Parameter `deviceDatum` arrives in `ecx`.
  - Follows the engine's tag resolution (`TLS` -> `threadCtx` -> `object_table` -> `tagIndex` -> `tag_globals` -> `tag_instances` -> `tagDef + 0x198`) to extract the 0-based `terminal_index`.
  - Also sets bit `terminal_index` in `campaign_progression` struct (`campaignStruct + 0x132`).
  - Emits `terminal_<index>:<scenario>` to `pendingEvents`.
  - Supported by `CheckPendingTerminals()` called in `Detour_Halo3EngineStep` on each simulation tick to poll `campaignStruct + 0x132` as an extra safeguard against missing any terminal reads.

### 11. Skull Award Hooks (`halo3.dll + 0x136088` & `+0x136580`)
* **Signatures**:
  - Primary: `void __fastcall AwardPrimarySkull(int32_t playerDatum, int32_t skullIndex)` (`+0x136088`)
  - Secondary: `void __fastcall AwardSecondarySkull(int32_t playerDatum, int32_t skullIndex)` (`+0x136580`)
* **Role**: The true engine functions that award metagame skulls when picked up in campaign.
* **Logic**:
  - Parameters: `playerDatum` in `ecx`, `skullIndex` in `edx`.
  - Queues `skull_primary_<index>:<scenario>` or `skull_secondary_<index>:<scenario>` to notify `Halo3Client.py`.

### 12. Mission Completion Hook (`halo3.dll + 0x1F2F7C`)
* **Signature**: `void __fastcall GameWon(void* rcxParam, int32_t edxParam)`
* **Role**: Halo Script evaluator invoked upon beating any campaign mission.
* **Logic**:
  - Sets `s_gameWonFired = true` and queues `game_won:<scenario>`.
  - Passes through to original `GameWon` so the engine can finalize scoring, stats, and achievements.

### 13. Next Campaign Level Interception & Clean Menu Exit (`halo3.dll + 0x1398E4` & `+0xF9114`)
* **Signatures**:
  - `void __fastcall LoadNextCampaignLevel(void* rcx, void* rdx, void* r8)` (`+0x1398E4`)
  - `void __fastcall Halo3_RequestSaveAndQuit(int32_t mode, float timeout, int32_t flags)` (`+0xF9114`)
* **Role**: Intercepts MCC's automatic campaign playlist advancement and cleanly returns the player to the MCC menu.
* **Logic**:
  - In vanilla Halo 3, completing a level calls `0xF05CA`, which invokes `0x1398E4` (`LoadNextCampaignLevel`) to transition into the next campaign mission (e.g. Sierra 117 &rarr; Crow's Nest).
  - Our detour on `0x1398E4` **suppresses the call to original**, completely halting the transition.
  - It immediately invokes `Halo3_RequestSaveAndQuit(0, 0.0f, 0)` at `+0xF9114` (the exact native C++ routine backing `(game_save_and_quit)`).
  - The engine cleanly saves campaign progression and exits directly to the MCC campaign menu without simulating keystrokes or UI interactions.

---

### Reverse Engineering Postmortem: Why Earlier Hooks Failed
1. **`halo3.dll + 0x5A2A30` (`unit_find_interaction_candidate`)**:
   - Only called during rare vehicle seating / turret detachment interactions; was never invoked during weapon aiming or ally trade aiming.
2. **`halo3.dll + 0x3B2740` (`UnitCanInteract`)**:
   - An action table function pointer that was never called directly during crosshair aiming (0 hits during gameplay).
3. **`halo3.dll + 0x91694` / `+0x8B52C` (`weapon_evaluate_candidate` / `unit_evaluate_candidate`)**:
   - Despite matching table strings in `.rdata` (`weapon-pickup`, `unit-pickup`), disassembly revealed these were actually Bungie network bitstream serialization callbacks (`and edx, 0x1; or rax, rdx; inc ecx`), called only during multiplayer gamestate network serialization, never during local campaign candidate selection.
4. **`halo3.dll + 0x38DB2C` / `+0x3B59EC` / `+0x3B6398`**:
   - Functions identified from candidate evaluation trace paths that were bypassed during standard single-player campaign crosshair lookups (0 hits in logs during aiming).
5. **The Real Candidate Evaluation Dispatch Table (`halo3.dll + 0x8B3970`)**:
   - Master object candidate evaluation table indexed by `objectType` with stride `0x28`:
     - Index 0 (`0x8B3970`): `halo3.dll + 0x3B7438` (`BipedEvaluateCandidate`) — evaluates bipeds/allies and directly invokes `chud_set_interaction_prompt`.
     - Index 1 (`0x8B3998`): `halo3.dll + 0x3B6DB4` (`VehicleEvaluateCandidate`) — evaluates vehicle interaction.
     - Index 2 (`0x8B39C0`): `halo3.dll + 0x3B5378` (`WeaponEvaluateCandidate`) — evaluates ground weapons. Returning `false` prevents candidate registration, suppresses HUD pickup/swap prompts, and prevents reload interruption.
6. **The Needler Auto-Pickup Bug (Single Weapon vs Dual-Wield)**:
   - When Chief has only 1 weapon (e.g. starter Assault Rifle) and an empty secondary slot, the engine passes `slotPref == 3` to `unit_action_pickup_weapon` (+0x3B2F18) to indicate secondary slot pickup.
   - A naive guard checking `if (slotPref == 3 && !IsWeaponDualWieldableByName(wname)) return false;` blocked all unlocked two-handed weapons (like the Needler) from being picked up into the empty secondary slot.
   - The correct condition only checks dual-wieldability when Chief **already has 2 weapons** (`hasPrimary && hasSecondary`), allowing normal pickup when an empty slot is available while correctly preventing the dual-wield failure loop when full.
7. **NPC Unarmed Weapon Stripping Regression**:
   - **What failed**: Gating `unit_can_pickup_weapon` (+0x35887C) or `unit_add_weapon` (+0x358024) without verifying `IsPlayerUnit(unitDatum)` stripped all locked weapons from friendly Marines and enemy Grunts/Brutes, causing them to spawn empty-handed.
   - **Fix**: Resolving Master Chief's true unit datum (`0xE2EC007D`) from the thread-local player table (`threadData + 0x10 -> +0x48 -> +0x28`) and restricting all weapon gating exclusively to `IsPlayerUnit(unitDatum)`.
8. **CHUD Multi-Slot Candidate Prompt Blanking Regression**:
   - **What failed**: In `chud_set_interaction_prompt` (+0x382D88), looping over all 4 candidate slots and returning `0` if *any* candidate slot was a locked weapon. If a player had a locked weapon in candidate slot 1, it wiped the prompt for an unlocked weapon in candidate slot 0.
   - **Fix**: Removed the detour on `chud_set_interaction_prompt` (+0x382D88) and enforced prompt suppression at the specific interaction generators (`unit_update_weapon_prompt` and `unit_update_trade_prompt`).
9. **Multi-Weapon Crate Interaction Abort vs Ground Prompt Isolation**:
   - **What failed**: In Halo 3, Covenant weapon containers hold multiple weapons in distinct slots (e.g. Plasma Pistol in slot 0, Needler in slot 1). In the container evaluator (`halo3.dll + 0x37FD30`), at `0x37FF9B`, it calls `unit_can_pickup_weapon` (+0x35887C) for the container's first weapon. If `unit_can_pickup_weapon` returns `false`, instruction `0x37FFA2: je 0x37FFCC` aborts the evaluation loop immediately, never evaluating subsequent weapons in the crate (the Needler).
   - **Regression**: Checking `isInContainer` (`weaponObj + 0x168 != 0xFFFFFFFF`) failed because in Halo 3, weapons in scenario crates do not have container datum pointers set on them.
   - **Resolution**: In `Detour_unit_can_pickup_weapon`, check `bool isCrateEval = (callerOffset >= 0x37FD30 && callerOffset <= 0x37FFC0)`. This caller range is used exclusively by the crate evaluator and is never called for dropped ground weapons. For crate evaluation, locked weapons pass through to prevent the crate loop from aborting, allowing the Needler to be evaluated and prompted. Dropped weapons on the ground call `+0xEA81C` or `+0xE9BC1`, where locked weapons are strictly blocked, cleanly suppressing their HUD prompts.
10. **Ally Weapon Trade HUD Prompt Suppression via `CanTradeWithAlly` (+0x3578C8)**:
    - **What failed**: Trying to suppress the trade prompt downstream at `biped_evaluate_candidate` (+0x3B7438), `unit_update_swap_prompt` (+0x3B62C0), or `unit_update_trade_prompt` (+0x3B6398). In these functions, the parameters are internal action structures or `playerIndex`, NOT the ally unit datum. Reading candidate table entries or searching for the ally failed because during raycasting the candidate had not yet been registered, or candidate lookups expected `playerIndex` instead of `chiefDatum`.
    - **Resolution**: Crosshair raycast candidate evaluation for allies is rooted at `halo3.dll + 0xEA8C8`. At instruction `0xEA9B3`, `0xEA8C8` calls `CanTradeWithAlly(chiefDatum, allyDatum)` at `halo3.dll + 0x3578C8`. If `CanTradeWithAlly` returns `false` (`0`), `0xEA8C8` immediately branches to `0xEAA17` and returns `0`, completely aborting trade candidate creation. Hooking `CanTradeWithAlly` (+0x3578C8), inspecting `allyDatum`'s inventory (`allyObj + 0x268`), and returning `false` if the ally holds a locked weapon prevents candidate Action 2 from ever being created. As a result, the "Hold [E] to trade weapons" prompt never renders, and pressing [E] cannot initiate a trade.
11. **Multi-Weapon Crate Evaluation & Secondary Slot Swap Caller Bypass**:
    - **What failed**:
      1. In `Detour_unit_can_pickup_weapon` (+0x35887C), caller `0x0EAD0F` was not bypassed. Caller `0x0EAD0F` (called from `0xEAC68` at `0xEAD0A`) tests whether Chief's *other* already-held inventory slot is valid before allowing a weapon swap. If Chief was holding an unauthorized starter weapon, `unit_can_pickup_weapon` returned `false`, aborting the swap check.
      2. In `Detour_unit_update_swap_prompt` (+0x3B62C0), `candidateData` was treated as an array of weapon datums. It is actually a 0x20-byte prompt descriptor; reading random dwords caused false-positive locked detections that suppressed legitimate prompts (like the Needler in a crate).
    - **Resolution**:
      1. In `Detour_unit_can_pickup_weapon`, allow pass-through for `isBypassCaller = (callerOffset >= 0x37FD30 && callerOffset <= 0x37FFC0) || (callerOffset == 0x0EAD0F) || (callerOffset == 0x091F8C)`. This allows the crate evaluation loop (`0x37FD30`) to evaluate all crate items without aborting on locked item 0 (Plasma Pistol), and allows secondary inventory checks (`0xEAD0F`) during swaps.
      2. Container candidate check (`0x0EAB47`) is strictly gated: locked weapons in containers return `false` (no prompt for Plasma Pistol), while unlocked weapons (Needler) return `true` (qualified for prompt).
      3. Removed fragile prompt detours on `0x3B62C0` and `0x3B6398`, allowing the engine's native candidate-driven prompt pipeline to render prompts for all legitimately qualified weapons.
12. **Dynamic Tag Table Boundary Scan Crash (8,192 vs 16,384 Tags)**:
    - **What failed**: An attempt to expand `ScanAndRegisterMapWeapons` loop bound to 16,384 tags caused an unhandled Access Violation crash during level loading immediately after scanning `plasma_cannon` (`0x1747`). In MCC, the runtime `tagInstances` memory page buffer does not span 16,384 entries for all maps; dereferencing entries past the mapped boundary or reading invalid tag offsets (`0xFFFFFFFF`) triggered an illegal memory read.
    - **Fix**: Reverted the tag loop to the safe 8,192 maximum with a safety `break` when encountering unmapped memory and explicit validation of `tagOffsetDword != 0xFFFFFFFF`. (All 22 campaign weapons on Sierra 117 reside well below index `0x1800`).
13. **Current State Diagnostic Analysis (Why Ally Trade Prompt Persists & Needler Prompt is Missing)**:
    - **Observed Behavior on Sierra 117**:
      - Level loads cleanly without crashes.
      - Dropped weapons on ground (Plasma Pistol): HUD prompt is correctly suppressed.
      - Action swap execution: Pressing/holding swap on locked weapons is successfully blocked.
      - Valid pickup (Spiker): Successfully picked up off the ground.
      - **BUG 1 (Ally Trade)**: Reticle over friendly Marine holding locked Magnum still displays `"Hold [E] to trade weapons"`.
      - **BUG 2 (Weapon Crate)**: Reticle over Covenant weapon container containing locked Plasma Pistol + unlocked Needler displays no prompt for either weapon.
    - **Definitive Resolutions Discovered via Ghidra Headless Decompilation**:
      1. **Ally Trade Prompt Generator Found (`FUN_18037fd30` @ `+0x37FD30`)**:
         - In `raycast_evaluate_candidate` (`+0xEA4EC`), lines 96–101 revealed that when Chief aims at an ally, it calls `FUN_18037fd30`. If this function returns true, it registers Candidate Action 7 (`CONCAT62(..., 7)`), which displays `"Hold [E] to trade weapons"`.
         - Inside `FUN_18037fd30`, at instruction `0x37FF9B`, it evaluates the ally's held weapon by calling `unit_can_pickup_weapon(Chief, allyWeapon, 4, ...)`.
         - **The Regression**: In `hook_manager.cpp`, we had previously added `bool isBypassCaller = (callerOffset >= 0x37FD30 && callerOffset <= 0x37FFC0)` under the mistaken assumption that `0x37FD30` was a weapon crate iterator. That bypass explicitly forced `unit_can_pickup_weapon` to return `true` whenever `0x37FF9B` queried the ally's weapon, regardless of Archipelago lock status!
         - **The Fix**: Removed `callerOffset >= 0x37FD30 && callerOffset <= 0x37FFC0` from `isBypassCaller`. When aiming at a Marine holding a locked Magnum, `unit_can_pickup_weapon` returns `false`, `FUN_18037fd30` returns `false`, and Candidate Action 7 is never added, cleanly suppressing the trade prompt.
      2. **Hook Re-installation Bug in `RemoveHalo3Hooks()`**:
         - `g_targetUnitUpdateWeaponPrompt` was omitted from `RemoveHalo3Hooks()`, leaving the MinHook entry active across level loads and triggering `MH_ERROR_ALREADY_CREATED` on subsequent map loads.
         - **The Fix**: Added explicit cleanup for `g_targetUnitUpdateWeaponPrompt` in `RemoveHalo3Hooks()`.

14. **Skull Award Stubs Crash (`+0x1E8F24` / `+0x1E8F50`) vs True Award Functions (`+0x136088` / `+0x136580`)**:
    - **What failed**: Early disassembly identified `halo3.dll + 0x1E8F24` and `+0x1E8F50` from string references. Hooking these crashed the game with an access violation immediately when picking up the Blind skull on Sierra 117.
    - **Resolution**: Static analysis revealed these were internal telemetry or networking stubs that did not follow standard fastcall parameter passing. The true engine functions executed upon skull acquisition are `halo3.dll + 0x136088` (`award_primary_skull`) and `halo3.dll + 0x136580` (`award_secondary_skull`), both accepting `(int32_t playerDatum, int32_t skullIndex)` in `ecx` and `edx`. Detouring these cleanly detects all 13 campaign skulls without crashes.

15. **The Skull & Oddball SMG StringID Collision (`0xE113`) & True Weapon Type Offset (`+0x18C`)**:
    - **What failed**: On `010_jungle.map`, `primary_skull.weapon` (`tagIndex 0x11F6`) and `secondary_skull.weapon` (`tagIndex 0x11DE`) compile with Switch-To StringID `0xE113` — the exact same StringID used by the SMG. In any seed where the SMG was not yet unlocked, dynamic tag scanning aliased both skulls as `"smg"`. As a result, `weapon_evaluate_candidate` and `unit_can_pickup_weapon` flagged the skulls as locked SMGs, suppressing "Hold [E] to pick up skull" prompts and completely blocking auto-pickup.
    - **Initial Attempt**: Earlier code attempted to check `tagDef + 0x33C` and `tagDef + 0x1AC` for melee reporting / melee-only flags. Memory inspection revealed these offsets held floating-point weapon range / aim-assist parameters, NOT flags, causing the check to fail.
    - **Resolution**: In MCC Halo 3, the true weapon type field is at **`tagDef + 0x18C`** (`uint16_t` / enum):
      - Skulls and Oddball: `*(uint16_t*)(tagDef + 0x18C) == 11` (`_weapon_type_ball`).
      - All firearms: `*(uint16_t*)(tagDef + 0x18C) != 11` (e.g. 0).
      - In `ScanAndRegisterMapWeapons`, tags with `weaponType == 11` are skipped so they are never registered as SMGs.
      - In `GetWeaponNameFromTag`, `weaponType == 11` returns `nullptr`.
      - In `IsTagUnlocked`, `weaponType == 11` unconditionally returns `true`, ensuring skulls can always be inspected, picked up, and carried.

16. **The True `campaign_level_is_last` Query (`+0xF01A0`) vs Bypassed `+0xF0104`, and MCC `RestartGameEngine` Teardown**:
    - **What failed**:
      1. Earlier code detoured `campaign_level_is_last` at `halo3.dll + 0xF0104`. Disassembly of `0xF053C` (mission completion dispatcher) revealed it does NOT call `+0xF0104`; it calls **`halo3.dll + 0xF01A0`** at instruction `0x60` (`call 0xF01A0; test al, al; jne 0x93`). Because `+0xF01A0` was not hooked, it returned `false`, causing `0xF053C` to fall through to `0x478F5` (which selected Crow's Nest `020_base`) and `0x1398E4` (`LoadNextCampaignLevel`).
      2. Earlier code attempted to suppress `MCC-Win64-Shipping.exe + 0x1E6424` (`RestartGameEngine`) by returning `void` early without calling the original function. This caused MCC's internal engine transition state machine to deadlock in limbo (neither running the game nor returning to the menu), which froze the process on Alt-F4 and forced a Task Manager kill.
    - **Resolution**:
      1. In `Detour_RestartGameEngine`, when `WasGameWonFired()` is true, the queued scenario string at `MCC_SCENARIO_STRUCT` (`mccBase + 0x03F8E778 -> +0x30`) is cleared to prevent MCC from holding any next level, and `g_originalRestartGameEngine` is invoked. This allows MCC's engine to execute its native teardown, cleanly unload `halo3.dll`, and transition smoothly back to the MCC main menu shell without hanging.
      2. Replaced global hooking of `0xF01A0` with a direct single-instruction patch at `0xF05A3` (see item 19).

17. **Rally Point JSON Backslash Escaping & Mission Tagging**:
    - **What failed**: `GetCurrentScenarioCode()` returned `levels\solo\010_jungle\010_jungle`. In `pipe_server.cpp`, building the response JSON used raw string concatenation (`"\"" + ev + "\""`), generating invalid JSON escape sequences (`\s`, `\0`). Python's `json.loads` threw `JSONDecodeError`, which was silently swallowed by `Halo3Client.py`, dropping all rally point events during Sierra 117. Later, when Crow's Nest started, the client evaluated the queued checks under Crow's Nest.
    - **Resolution**:
      1. In `event_hooks.cpp`, all backslashes in `GetCurrentScenarioCode()` are normalized to forward slashes `/`.
      2. In `pipe_server.cpp`, `EscapeJsonString()` escapes backslashes and double quotes in all pending events.
      3. In `Halo3Client.py`, `ack_str.replace('\\', '/')` is applied defensively, and `JSONDecodeError` is logged with `logger.error`. Events are attributed immediately to the correct mission.

18. **Outro Cutscene Unit Null Pointer Dereference Crash (`halo3.dll + 0x355EB9`)**:
    - **What failed**: At the conclusion of Sierra 117, Crow's Nest loading was skipped, playing the outro Pelican evacuation cutscene. Approximately 18 seconds later, as Chief's parent vehicle/pelican despawned during the transition, the camera update routine (`0xF4C99` &rarr; `0xE3A48` &rarr; `0x355E20`) executed `unit_get_seat_camera` (+0x355E20).
      - Unlike its sibling function `unit_get_parent_camera` (+0x355CD4), which checks `cmp [rcx+0x10], 0xFFFFFFFF; je return`, `0x355E20` had no validation on `[rcx+0x10]` (parent datum) or on the resolved object pointer `[r9+rcx*8+0x10]`.
      - With `parentDatum == 0xFFFFFFFF`, the engine loaded an uninitialized slot (`0xFFFF`) returning `nullptr`, and instruction `0x355EB9` (`movzx ecx, WORD PTR [rax]`) dereferenced `nullptr` &rarr; fatal exception `0xC0000005` (Access Violation).
    - **Resolution**:
      1. Detoured `halo3.dll + 0x355E20` (`Detour_UnitGetSeatCamera`).
      2. Validates that `unitDatum` is valid, `unitObj` is non-null, `parentDatum` is valid (`!= 0xFFFFFFFF && != 0`), and `parentObj = GetObjectPtr(g_halo3Base, parentDatum)` is non-null.
      3. Validates that `parentObj` has a loaded tag definition offset before calling original code.
      4. If any check fails, returns `false` (`0`), allowing the camera update to fall back safely to normal player coordinates without crashing.

19. **Post-Game Stats Calculation Crash (`halo3.dll + 0x132140`) & Replacing Global `0xF01A0` Hook with `0xF05A3` Branch Patch**:
    - **What failed**: Hooking `campaign_level_is_last` (+0xF01A0) globally caused an Access Violation `0xC0000005` at `halo3.dll + 0x132140` during post-game mission scoring.
      - `0xF01A0` is called by over 150 functions across `halo3.dll`.
      - In `0x1320BC` (post-game player stats update), instruction `0x132126` computes `imul r9, r14, 0x518` immediately before `call 0xF01A0` (`0x13212D`).
      - Because vanilla `0xF01A0` is an 8-instruction leaf function that never touches `r9`, MSVC's Whole Program Optimization (/GL) kept `r9` live across the call and used it immediately after at `0x132140` (`test BYTE PTR [r9+r11*1+0xfaf], 0x1`).
      - The MinHook detour for `0xF01A0` invoked C++ code and logging, which legitimately clobbered volatile register `r9`. Upon return, `r9` contained garbage, dereferencing invalid memory at `0x132140`.
    - **Resolution**:
      1. Completely removed the hooks on `0xF01A0` (`campaign_level_is_last`) and `0xF0104`.
      2. Directly patched instruction `halo3.dll + 0xF05A3` (`75 2A` = `jne +0x2A`) to `EB 2A` (`jmp +0x2A`).
      3. In `game_won_handler` (`0xF053C`), this forces the engine to always jump directly to `+0xF05CF` (clean campaign completion branch), skipping `0x137E98` and `0x1398E4` (`LoadNextCampaignLevel`) without modifying or intercepting `0xF01A0` for any other callers in the engine.
      4. `r9` remains 100% intact across all stats calculations, eliminating the crash at `0x132140`.

---

## 5. Dynamic Weapon Tag Scanning & StringID Table

In Halo 3, weapon tag datum indices vary from map to map because each map compiles its own tag index dictionary. However, the tag definition header contains a **Switch-To StringID** at offset `+0x11C`.

These StringIDs belong to **Set 0xE** (global engine strings compiled into `halo3.dll`) and are **100% constant across every campaign map in the game**:

| StringID (Hex) | Weapon Canonical Name |
|---|---|
| `0xE112` | `assault_rifle` |
| `0xE113` | `smg` |
| `0xE114` | `covenant_carbine` |
| `0xE116` | `battle_rifle` |
| `0xE117` | `beam_rifle` |
| `0xE119` | `rocket_launcher` |
| `0xE11A` | `machinegun_turret` |
| `0xE11D` | `sentinel_beam` (Variant) |
| `0xE11F` | `flak_cannon` |
| `0xE120` | `flamethrower` |
| `0xE122` | `shotgun` |
| `0xE123` | `magnum` |
| `0xE124` | `missile_pod` |
| `0xE125` | `needler` |
| `0xE126` | `gravity_hammer` |
| `0xE129` | `plasma_pistol` |
| `0xE12A` | `plasma_rifle` |
| `0xE12B` | `sniper_rifle` |
| `0xE12C` | `mauler` |
| `0xE12D` | `plasma_cannon` |
| `0xE12E` | `sentinel_beam` (Standard) |
| `0xE12F` | `brute_shot` |
| `0xE130` | `spartan_laser` |
| `0xE131` | `energy_sword` |
| `0xE132` | `spiker` |

### Tag Table Pointers & Offsets
- Tag instances table pointer: `halo3.dll + 0xA49018`
- Tag cache base pointer: `halo3.dll + 0x1FCF4C8`
- Tag definition lookup: `tagCacheBase + (uintptr_t)tagOffsetDword * 4`
- Unit inventory offset: `Chief Object + 0x268` (4 slots of 4 bytes each)

### Tag Definition (`weap`) Internal Layout
- `tagDef + 0x000` (uint16_t): Object Type (`0x0002` = Weapon)
- `tagDef + 0x11C` (uint32_t): Switch-To StringID (`s11c`, Set 0xE)
- `tagDef + 0x18C` (uint16_t): Weapon Type (`11` = `_weapon_type_ball` for skulls and oddball; firearms != 11)

---

## 6. Reverse Engineering with Ghidra
 
Ghidra (v12.1.4 Public or later) serves as the primary tool for static analysis, decompilation, and function signature recovery across both `MCC-Win64-Shipping.exe` and `halo3.dll`.
 
### 6.1 Headless Execution Mode
 
* **Tool Location**: `<ghidra_install_dir>/support/analyzeHeadless.bat`
* **Purpose**: Enables programmatic headless scripting in Java or Python (Jython) to automate batch analysis, symbol searching, and decompilation directly from the command line without launching a GUI.
* **Basic Syntax**:
  ```cmd
  analyzeHeadless.bat <project_directory> <project_name> -import <binary_path> [-postScript <script_path>]
  ```
* **Existing Project Analysis**:
  ```cmd
  analyzeHeadless.bat <project_directory> <project_name> -process <binary_filename> -postScript <script_path>
  ```

### 6.2 Key Binary Layouts & Base Addresses

| Binary | Default File Path | Ghidra Default ImageBase | Memory Address Calculation |
|:---|:---|:---|:---|
| `MCC-Win64-Shipping.exe` | `<SteamMCC>\MCC-Win64-Shipping.exe` | `0x140000000` | `RVA = Address - 0x140000000` |
| `halo3.dll` | `<SteamMCC>\halo3\halo3.dll` | `0x180000000` | `RVA = Address - 0x180000000` |

*Note: In `offsets.h` and the C++ mod code, all offsets are Relative Virtual Addresses (RVAs) from the base of the loaded module. Always subtract the Ghidra image base (`0x180000000` for `halo3.dll`) when extracting hook targets.*

### 6.3 Proven Ghidra Reverse Engineering Workflows

#### 1. Locating Halo Script (HS) Built-in Evaluators
Halo 3 maps its campaign script commands (e.g. `game_won`, `game_save_and_quit`, `terminal_was_accessed`, `game_insertion_point_unlock`) through a centralized internal symbol table.
* **Workflow**:
  1. Open **Search &rarr; Program Text** (or Search Strings) and query the command name in `.rdata` (e.g. `"game_won"`).
  2. Find references (XREFs) to the string pointer: this leads to a table entry struct `(char* name, uint16_t type, void* evalFunc, ...)`.
  3. The function pointer in this struct points directly to the engine's HS implementation or dispatch wrapper.
  4. Decompile the evaluator to find callers or internal engine dispatch functions (such as tracing `game_save_and_quit` to `Halo3_RequestSaveAndQuit` at `+0xF9114`).

#### 2. Tracing Interaction Candidate VTables & HUD Prompt Generators
Interaction prompts in Halo 3 are governed by candidate evaluators registered in interaction vtables.
* **Workflow**:
  1. Search for diagnostic strings compiled into Bungie's debug logging (e.g. `"weapon-pickup : relevance=%5.3f"`, `"unit-pickup"`, `"biped-pickup"`).
  2. Follow XREFs to find the `c_player_select_actions_datasource` initialization and the candidate vtable definitions (`.data` / `.rdata`).
  3. Method 2 in these vtables corresponds to `evaluate_candidate`:
     - Weapon candidate evaluator: `+0xEA6E4`
     - Ally candidate raycaster: `+0xEA8C8` / `+0x3578C8` (`CanTradeWithAlly`)
     - Crate evaluator: `+0x37FD30`

#### 3. Analyzing Level Transitions & Campaign Flow
* **Workflow**:
  1. Set a decompilation bookmark on `game_won` (`+0x1F2F7C`).
  2. Follow calls out of `game_won` into `0xF053C` (mission completion handler).
  3. Inspect the branch logic to discover where next-map loading is scheduled:
     - `0xF05CA` calls `+0x1398E4` (`LoadNextCampaignLevel`).
     - Searching callers to `0x1398E4` confirms it is called exclusively from `0xF05CA`, proving it is the single bottleneck for advancing campaign levels.

#### 4. Scaleform / UI Flash Boundary Reverse Engineering
MCC campaign menus run in Scaleform GFx. C++ interacts with Flash via ActionScript dispatchers.
* **Workflow**:
  1. Search for menu action hashes in `.text` (e.g. `0xF3A4CF96` for submenu navigation, `0xAE68D666` for direct launch).
  2. Search for Scaleform array paths (`root.Container.mcArray.array`, `root.mcScreen.mcContainer.mcArray.array`).
  3. Inspect functions referencing these paths to map how MCC populates game lists and reads lobby settings.

---

## 7. Campaign Completion Transition & Process Exit Lifecycle

### 7.1 MCC Campaign Level Advancement vs. Clean Menu Return
* **Function**: `RestartGameEngine` (`MCC-Win64-Shipping.exe + 0x1E6424`)
* **Signature**: `void __fastcall RestartGameEngine(void* pThis, int32_t reason, void* r8, void* r9)`
* **Engine Completion Flow**:
  1. When a campaign mission completes, the campaign script calls `game_won` (`halo3.dll + 0x1F2F7C`).
  2. `game_won` dispatches to the engine mission completion handler at `halo3.dll + 0xF053C`.
  3. At `0xF05A3`, the engine performs a conditional branch (`75 2A` / `jne +0x2A`) whether to invoke `LoadNextCampaignLevel` (`+0x1398E4` at `0xF05CA`).
  4. If next-level loading proceeds, MCC subsequently triggers `RestartGameEngine` with `reason = 0` to reload `halo3.dll` for the next playlist map.
* **The Archipelago Solution**:
  - **Patch 0xF05A3 (Bypass Next Level Call)**:
    - We patch `0xF05A3` from `75 2A` (`jne +0x2A`) to `EB 2A` (`jmp +0x2A`).
    - This skips `LoadNextCampaignLevel` (`+0x1398E4`) entirely and directly branches to the session conclusion / teardown routine at `+0xF05CF`.
  - **Patch 0x12EBC4 (`main_game_load_next_level` -> ret false)**:
    - As defense-in-depth, `main_game_load_next_level` is patched with `xor al, al; ret` (`30 C0 C3`).
    - When `main_game_load_next_level` returns `false` at `+0x12E615`, `halo3.dll` enters its natural error-recovery fallback at `+0x12E619`.
    - This fallback invokes internal game state recovery routines (`0x180036EB0` with `arg=2`, `0x1802BDCBC`, `0x18012FB1C`), clears the completion flag at `0x1821A159D`, and cleanly puts `halo3.dll` into an idle state.
  - **Detour `RestartGameEngine` (`mcc_mod/hooks/lobby_hooks.cpp`)**:
    - When `WasGameWonFired()` is true during `RestartGameEngine`:
      1. Zero out the queued scenario string in `MCC_SCENARIO_STRUCT` (`mccBase + 0x3F8E778 -> +0x1A`) so MCC does not attempt to reload the mission or schedule the next scenario.
      2. Preserve MCC's original `reason` and parameters so MCC's internal state machine processes the restart teardown normally.
      3. Call `ResetEventState()` to clear `s_gameWonFired`.
      4. Forward to `g_originalRestartGameEngine(pThis, reason, r8, r9)`.
* **Pitfalls Encountered & Debunked Approaches**:
  - *Forcing Campaign End via NOP at `+0x12E5AB`*: Attempting to NOP the branch at `0x12E5AB` forced the "campaign end" path (`session->end(mode=1)` via vtable `*0x1B0(%r9)`). This caused a permanent black screen because MCC's campaign-end flow (PGCR screen, credits) is only valid for the final campaign level (`Halo`), deadlocking on non-final levels like Sierra 117.
  - *Forcing `reason = 6` / Scaleform PGCR event `0x4a2be129`*: Overwriting the restart reason to 6 or dispatching PGCR events manually also resulted in a black screen because MCC's lobby shell was not in a state expecting PGCR for a mid-campaign mission.
  - *The working formula*: Letting halo3's natural fallback idle the engine, zeroing the scenario string in `MCC_SCENARIO_STRUCT`, and letting MCC process `RestartGameEngine` with its original parameters.

### 7.2 Game Exit & Process Detach Crash Postmortem (`dxgi.dll + 0x3534`)
* **Symptom**: Game crashed or hung on Alt-F4 / normal exit with Access Violation `0xC0000005` at `dxgi.dll + 0x3534`.
* **Disassembly at Crash Point**:
  ```asm
  cmp BYTE PTR [rbx+0xf05a3], 0xeb  ; rbx = g_halo3Base (unmapped pointer)
  ```
* **Root Cause**:
  1. During process termination (`ExitProcess` or Alt-F4), the Windows OS unloads loaded DLLs. `MCC-Win64-Shipping.exe` unloaded `halo3.dll` *before* `dxgi.dll`'s `DllMain` ran `DLL_PROCESS_DETACH`.
  2. `RemoveHalo3Hooks()` checked `if (g_halo3Base)` and attempted to read `*pPatch == 0xEB` to restore original bytes. Because `halo3.dll` had already been unmapped, this dereferenced freed virtual memory, throwing `0xC0000005`.
  3. Throwing an unhandled exception inside `DllMain(DLL_PROCESS_DETACH)` deadlocked the Windows LoaderLock, hanging the process on Alt-F4 until terminated via Task Manager.
* **The Fix**:
  1. In `DllMain` (`mcc_mod/mod_main.cpp`), check `lpReserved != nullptr`. During process termination, do NOT call `RemoveHooks()`, `MH_Uninitialize()`, or `FreeRealDXGI()`; per Microsoft guidance, allow the OS kernel to instantly and cleanly reclaim the entire address space.
  2. In `RemoveHalo3Hooks()` (`mcc_mod/hook_manager.cpp`), guard all unhooking and patch reversals with `GetModuleHandleA("halo3.dll") == (HMODULE)g_halo3Base` and `VirtualQuery(pPatch, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT`.


