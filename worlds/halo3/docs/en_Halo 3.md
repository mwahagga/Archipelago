# Halo 3

[Setup Guide](setup_en.md)

## What is Halo 3 Archipelago?

Halo 3 Archipelago is a randomizer world for the campaign of **Halo 3** in **Halo: The Master Chief Collection (MCC)**. In this multiworld randomizer:

- Campaign missions, skulls, terminals, rally points, and weapon licenses can be shuffled across the multiworld.
- Progress in your Halo 3 campaign sends checks to unlock items and progression for other players in your multiworld session, while receiving items unlocks new levels and arsenal in real-time.

---

## Where are the player options?

Player options are configured in your player YAML settings file (such as `Players/Halo3Test.yaml`) or generated using the Archipelago Launcher options generator. Available options include weapon license modes, included skulls, terminals, rally points, and goal conditions.

---

## What does randomization do to this game?

### 1. Mission Progression Randomizer
By default, you start with one mission unlocked (typically **Sierra 117**, or a randomized starting mission). All other missions are locked by the native mod proxy until their respective mission unlock items are found in the multiworld. Trying to launch a locked mission will be blocked until unlocked.

### 2. Weapon License System (Optional)
- **Disabled**: All campaign weapons can be picked up and fired normally.
- **Individual Licenses**: Every weapon (e.g. Battle Rifle, Sniper Rifle, Energy Sword, Rocket Launcher) requires its specific license item from the multiworld. Unlicensed weapons cannot be picked up until their license is unlocked.
- **Tiered Licenses**: Weapons are grouped into tiers (Common, Mid-Tier, Heavy/Power, Exotic). Unlocking a tier license unlocks all weapons in that tier.
- Support weapons (mounted machine gun turrets, missile pods, plasma cannons) remain usable by default.

### 3. Campaign Checks
- **Mission Completion**: Awarded upon completing a mission and reaching the Post-Game Carnage Report (PGCR).
- **Rally Points (Optional)**: Reaching Rally Point Bravo and Rally Point Charlie awards checks.
- **Terminals (Optional)**: Accessing any of the 7 story terminals on Legendary or Normal difficulty sends a check upon activation.
- **Campaign Skulls (Optional)**: Picking up any of the primary or secondary hidden skulls throughout the campaign sends a check immediately upon collection.

---

## What is the goal?

Configured via the **Goal** option in your YAML:
- **Beat Halo (Default)**: Defeat the final encounter on the mission *Halo* and escape the Ark.
- **Beat All Missions**: Complete all 9 campaign missions in Halo 3.

---

## What items can appear in other worlds?

- **Mission Unlocks**: Unlocks access to campaign missions (Crow's Nest, Tsavo Highway, The Storm, Floodgate, The Ark, The Covenant, Cortana, Halo).
- **Weapon Licenses**: Unlocks individual weapons or weapon tiers.
- **Traps / Fillers**: Optional filler items or gameplay modifiers.

---

## What do items from other players look like?

When other players find your mission or weapon licenses, the Archipelago Client sends a live update through the local TCP socket to the native mod. Your missions and weapon licenses unlock immediately in-game without needing to restart the game or reload the menu.

---

## Anti-Cheat Safety

The mod operates exclusively when Halo: MCC is launched in **Anti-Cheat Disabled** mode (`-eac-nop-loaded`). The Archipelago Client automatically manages the deployment and post-game removal of the mod file (`dxgi.dll`), leaving your MCC directory clean and ready for official multiplayer when you are done playing.
