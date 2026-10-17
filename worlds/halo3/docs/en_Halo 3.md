# Halo 3

[Setup Guide](setup_en.md)

## What is Halo 3 Archipelago?

Halo 3 Archipelago is a randomizer world for the campaign of **Halo 3** in
**Halo: The Master Chief Collection (MCC)**. In this multiworld randomizer:

- Campaign missions, skulls, terminals, rally points, and weapon licenses can be
  shuffled across the multiworld.
- Progress in your Halo 3 campaign sends checks to unlock items and progression
  for other players in your multiworld session, while receiving items unlocks
  new levels and arsenal in real-time.

---

## Where are the player options?

Player options are configured in your player YAML settings file (such as
`Players/Halo3Test.yaml`) or generated using the Archipelago Launcher options
generator. Available options include weapon license modes, included skulls,
terminals, rally points, and goal conditions.

---

## What does randomization do to this game?

### 1. Mission Progression Randomizer

By default, you start with one mission unlocked (typically **Sierra 117**, or a
randomized starting mission). All other missions are locked by the native mod
proxy until their respective mission unlock items are found in the multiworld.
Trying to launch a locked mission will be blocked until unlocked.

### 2. Weapon License System (Optional)

- **Disabled**: All campaign weapons can be picked up and fired normally.
- **Individual Licenses**: Every weapon (e.g. Battle Rifle, Sniper Rifle, Energy
  Sword, Rocket Launcher) requires its specific license item from the
  multiworld. Unlicensed weapons cannot be picked up until their license is
  unlocked.
- **Tiered Licenses**: Weapons are grouped into tiers (Common, Mid-Tier,
  Heavy/Power, Exotic). Unlocking a tier license unlocks all weapons in that
  tier.
- Support weapons (mounted machine gun turrets, missile pods, plasma cannons)
  remain usable by default.

### 3. Progressive MJOLNIR Shield Upgrades (Optional)

Controls Master Chief's shield capacity throughout the campaign:

- **Vanilla**: 100% full shields from the start. No shield items in the pool.
- **Progressive**: Start at 25% shields. Find 3 upgrades to reach 100% (50%,
  75%, 100%).
- **Progressive + Overshield**: Start at 25% shields. Reach 100% at Upgrade 3,
  then unlock Half Overshield (150%), Full Overshield (200%), Super Overshield
  (250%), and Triple Overshield (300%) with Upgrades 4-7.
- **Glass Cannon**: Start at 0% shields (health only, true glass cannon). Find 4
  upgrades to reach 100% (25%, 50%, 75%, 100%).
- **Glass Cannon + Overshield**: Start at 0% shields. Reach 100% at Upgrade 4,
  then unlock Half Overshield (150%), Full Overshield (200%), Super Overshield
  (250%), and Triple Overshield (300%) with Upgrades 5-8.

Each **Progressive Shield Upgrade** grants MJOLNIR shield capacity in real-time.
When overshields (> 100%) are unlocked, shields automatically recharge up to
your unlocked overshield maximum after staying out of combat for 5 seconds.
Shield recharge loops cleanly cease playing once your current shield tier is
fully charged.

### 4. Campaign Checks

- **Mission Completion**: Awarded upon completing a mission and reaching the
  Post-Game Carnage Report (PGCR).
- **Rally Points (Optional)**: Reaching Rally Point Bravo and Rally Point
  Charlie awards checks.
- **Terminals (Optional)**: Accessing any of the 7 story terminals on Legendary
  or Normal difficulty sends a check upon activation.
- **Campaign Skulls (Optional)**: Picking up any of the primary or secondary
  hidden skulls throughout the campaign sends a check immediately upon
  collection.

### 5. DeathLink (Optional)

When enabled, your survival is linked to other players in the multiworld session:
dying in combat sends a DeathLink to your peers, and their deaths trigger in-game
penalties for Master Chief.

- **`death_link`** (`Toggle`): Enable or disable DeathLink participation.
- **`death_link_amnesty`** (`Range`, 0–10): Number of local combat deaths to
  forgive before broadcasting a death outward to the multiworld (default `0`).
- **`death_link_receive_hits`** (`Range`, 1–5): Number of incoming DeathLinks
  required from the multiworld before triggering an in-game penalty on Chief (default `1`).
- **`death_link_effect`** (`Choice`): Selectable in-game penalty:
  - **Kill**: Instant death and checkpoint reload (classic hardcore DeathLink).
  - **Shield Break**: Instant EMP shock that depletes MJOLNIR shields to 0% with
    an immediate recharge delay reset.
  - **Mortal**: Depletes shields to 0% and reduces health to 10% (one hit from death).

Penalties received while in menus or loading screens are preserved and executed
cleanly once Master Chief spawns into active gameplay. Checkpoint reloads and
incoming penalty deaths never trigger outgoing death broadcasts.

---

## What is the goal?

Configured via the **Goal** option in your YAML:

- **Beat Halo (Default)**: Defeat the final encounter on the mission _Halo_ and
  escape the Ark.
- **Beat All Missions**: Complete all 9 campaign missions in Halo 3.
- **Fast Victory**: Complete any $N$ unique campaign missions in any order
  (customizable from 1 to 9 via `missions_required`, default 5).

---

## What items can appear in other worlds?

- **Mission Unlocks**: Unlocks access to campaign missions (Crow's Nest, Tsavo
  Highway, The Storm, Floodgate, The Ark, The Covenant, Cortana, Halo).
- **Weapon Licenses**: Unlocks individual weapons or weapon tiers.
- **Progressive Shield Upgrade**: Upgrades Master Chief's MJOLNIR shield
  capacity by +25% per item.
- **Traps / Fillers**: Optional filler items or gameplay modifiers (e.g. Moa
  Burger).

---

## What do items from other players look like?

When other players find your mission or weapon licenses, the Archipelago Client
sends a live update through the local TCP socket to the native mod. Your
missions and weapon licenses unlock immediately in-game without needing to
restart the game or reload the menu.

---

## Anti-Cheat Safety

The mod operates exclusively when Halo: MCC is launched in **Anti-Cheat
Disabled** mode (`-eac-nop-loaded`). The Archipelago Client automatically
manages the deployment and post-game removal of the mod file (`dxgi.dll`),
leaving your MCC directory clean and ready for official multiplayer when you are
done playing.
