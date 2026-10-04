# Halo 3 for Archipelago Setup Guide

[Game Info](en_Halo%203.md)

This guide covers setting up and playing **Halo 3** with the **Archipelago Multiworld Randomizer**.

---

## Required Software

- **[Halo: The Master Chief Collection (Steam)](https://store.steampowered.com/app/976730/Halo_The_Master_Chief_Collection/)**
  - Must have the **Halo 3** DLC installed.
  - The Steam version is recommended and supported. (The Windows Store / PC Game Pass version has strict UWP filesystem permissions and is not supported).
- **Archipelago** (version 0.6.7 or later).
- **Halo 3 APWorld (`halo3.apworld`)**:
  - Download `halo3.apworld` and place it into your Archipelago installation's `custom_worlds/` directory (or `lib/worlds/`).
  - The native mod (`dxgi.dll`) is bundled directly inside the `.apworld` archive—no external manual DLL installation required.

---

## Anti-Cheat Safety & How It Works

Halo: MCC uses EasyAntiCheat (EAC) for official matchmaking. 343 Industries officially provides an **Anti-Cheat Disabled** launch option specifically for community modding. The Archipelago integration operates strictly within this mode:

1. **Automated Lifecycle**: When launching via the Archipelago Client or opening a `.aphalo3` patch file, the client extracts `dxgi.dll` to MCC's `win64` directory, launches MCC with `-eac-nop-loaded`, and starts an independent cleanup supervisor.
2. **Guaranteed Post-Game Cleanup**: The detached cleanup supervisor monitors MCC's process. When the game terminates—even if the client is abruptly closed or your PC powers down—the supervisor safely deletes `dxgi.dll` from the MCC directory.
3. **In-DLL Failsafe**: Even if MCC were launched with Anti-Cheat active while `dxgi.dll` is present, the mod inspects its launch arguments on initialization. If `-eac-nop-loaded` is not present, it stays completely inert as a passthrough proxy without installing any hooks or creating IPC endpoints.
4. **Pre-Flight Sanitizer**: Whenever the Archipelago Client initializes, it sweeps the MCC directory to ensure no stale mod files were left behind from previous unexpected crashes.

---

## Installation & Configuration

### 1. Automatic Detection
By default, Archipelago will automatically locate your MCC installation by querying the Windows Registry, Steam library configurations (`libraryfolders.vdf`), or standard Linux paths.

### 2. Custom MCC Path (Optional)
If you have MCC installed on a secondary drive or non-standard directory, you can configure the path in your `host.yaml` file:

```yaml
halo3_settings:
  mcc_install_directory: "D:/SteamLibrary/steamapps/common/Halo The Master Chief Collection"
```

You can also specify the path manually at launch using `/launch <path>`.

---

## Starting a Game

### Method A: Open Generated Patch File (`.aphalo3`) (Recommended)

When generating a game, Archipelago produces a `.aphalo3` patch file for your player slot (e.g. `AP_12345_P1_Chief.aphalo3`).

1. Open the **Archipelago Launcher**.
2. Click **Open Patch** and select your `.aphalo3` file (or double-click / drag-and-drop the file onto the Launcher).
3. The launcher will automatically:
   - Extract your player name and server connection details.
   - Clean any stale mod files.
   - Deploy `dxgi.dll` into MCC's `win64` directory.
   - Launch MCC with Anti-Cheat Disabled.
   - Start the detached cleanup watchdog.
   - Connect the client to your multiworld game.

---

### Method B: One-Click Launch via Launcher GUI

1. Open the **Archipelago Launcher**.
2. Click **Halo 3 (Launch Game & Client)**.
3. Archipelago will:
   - Clean any stale mod files.
   - Deploy `dxgi.dll` into MCC's `win64` directory.
   - Launch MCC with Anti-Cheat Disabled.
   - Start the detached cleanup watchdog.
   - Start the Halo 3 Archipelago Client.
4. In the client console or GUI, connect to your multiworld server:
   ```text
   /connect <server_address>:<port> [password]
   ```
5. Enter your player slot name when prompted.

---

### Method C: Manual Launch

If you prefer to start MCC manually:

1. Launch Halo: MCC with **Anti-Cheat Disabled**:
   - In Steam, click **Play** and select **"Play Halo: The Master Chief Collection (Anti-Cheat Disabled)"** from the popup.
   - Or run `MCC-Win64-Shipping.exe -eac-nop-loaded` directly.
2. Launch the client:
   - In the Archipelago Launcher, click **Halo 3 Client**.
3. In the client, run `/launch` to deploy the mod and attach to MCC, or let the client auto-detect the running game.

---

## In-Game Checks & Randomizer Mechanics

### Campaign Missions
- Unlocked missions are available in the Campaign menu. Locked missions cannot be started until their unlocks are received from the multiworld.
- Completing a mission awards a check when you reach the **Post-Game Carnage Report (PGCR)** screen.

### Rally Points (Optional)
- If enabled in your YAML options, reaching **Rally Point Bravo** and **Rally Point Charlie** in campaign missions sends checks.

### Terminals (Optional)
- If enabled, interacting with any of the 7 story terminals sends a check upon activation.

### Skulls (Optional)
- Picking up any primary or secondary campaign skull sends a check immediately upon collection.

### Weapon Licenses (Optional)
- If Weapon License Randomizer is enabled (Individual or Tiered), weapons that you haven't unlocked yet are restricted.
- Receiving a weapon license from the multiworld instantly unlocks that weapon or weapon tier for the rest of your run.

---

## Client Commands Reference

| Command                | Description                                                                                                  |
| ---------------------- | ------------------------------------------------------------------------------------------------------------ |
| `/launch [path]`       | Deploys mod, launches MCC with Anti-Cheat disabled, and starts the cleanup watchdog.                         |
| `/clean_mod`           | Safely removes `dxgi.dll` from MCC to restore clean vanilla state.                                           |
| `/mod_status`          | Shows MCC install path, game process status, and mod installation status.                                    |
| `/status`              | Shows connection status, unlocked missions, weapon licenses, and completed checks.                           |
| `/complete <location>` | Manually marks a location or mission as completed (e.g., `/complete Sierra 117` or `/complete Blind Skull`). |
| `/connect_mcc`         | Immediately scans for and reconnects to the MCC process.                                                     |

---

## Steam Deck & Linux (Proton) Setup (Experimental & Untested)

> **Note**: While Linux and Steam Deck support is architected using cross-platform Localhost TCP sockets (`127.0.0.1:29500`), it is currently **untested in live play**. Feedback, bug reports, and testing assistance are welcome!

### 1. Steam Launch Option
Because Proton uses Wine's built-in DirectX libraries by default, you must configure Steam to allow the native `dxgi.dll` proxy to load:

1. In your Steam Library, right-click **Halo: The Master Chief Collection** and select **Properties**.
2. In the **General** tab, find **Launch Options**.
3. Add the following override command:
   ```text
   WINEDLLOVERRIDES="dxgi=n,b" %command%
   ```
4. Close the Properties window.

### 2. Archipelago Client on Linux
The client communicates with the game mod over a localhost TCP socket (`127.0.0.1:29500`). This bridges across the Proton prefix boundary into native Linux:
- You can run the Archipelago Client either natively on Linux or within the same Proton prefix.
- The client automatically detects Steam Deck paths (`~/.local/share/Steam` and Flatpak Steam).

---

## Troubleshooting & FAQ

### Q: How do I verify that the mod was cleanly removed?
Run `/mod_status` in the Halo 3 client, or check your `mcc/binaries/win64/` directory. There should be no `dxgi.dll` file present when the game is closed.

### Q: What happens if MCC or my computer crashes during a game?
The next time you open the client, its **Pre-Flight Sanitizer** will detect that MCC is not running and immediately remove any leftover `dxgi.dll`. Additionally, the detached watchdog will clean up as soon as the MCC process terminates.

### Q: The client says "MCC Process Connected: NO".
Make sure you launched MCC in **Anti-Cheat Disabled** mode (`-eac-nop-loaded`). In standard EAC mode, MCC prevents DLL injection and hooks from functioning. On Linux/Steam Deck, make sure `WINEDLLOVERRIDES="dxgi=n,b" %command%` is set in Launch Options.

### Q: Can I play with a friend in co-op?
Co-op campaign randomizer is currently **untested**. While the native mod and client allow multiple players to connect to the same multiworld room, synchronization of mission locks and weapon pickups across MCC's peer-to-peer co-op networking has not been tested or verified. Solo campaign play is recommended.
