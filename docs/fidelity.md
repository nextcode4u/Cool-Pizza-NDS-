# Gameplay and DS behavior

Cool Pizza DS follows the wave layouts and combat rules of the original iOS
version 1.4. Gameplay runs on the top screen; the bottom screen shows wave,
score and ghost-power information and accepts attack/power taps.

## Ramps and attacks

Ramps launch the skater automatically. Attacks are available while airborne and
use a six-tick cooldown. Hits and kills provide upward momentum, letting the
player chain enemies into a longer flight. Landing resets the air combo.

Each enemy has its own hit points and score value. Airborne kills increase the
combo multiplier, which multiplies the enemy's score. An unshielded attack
collision ends the run.

## Enemies

| Enemy | Behavior |
| --- | --- |
| Flies | Figure-eight hovering and authored movement paths |
| Bombing eyes | Drop bombs that explode near the ground |
| Sweepers | Track the skater, charge and fire a ground attack |
| Hands | Strike downward in their own lanes |
| Pyramids | Pull the skater toward them and launch a blast |

### Hand collision

A hand's damage hitbox is active for **0.25 seconds after impact**. It is inactive
during preparation, descent and return. Contact from any direction during the
active window can kill the skater, including falling onto the fist.

## Ghost powers

Air combos charge the ghost's powers in order: jump, shot, nuke and shield.
Press Y or tap the power button to use the available power. A grounded attack
press also activates it.

| Power | Effect |
| --- | --- |
| Jump | Launches the skater upward |
| Shot | Fires an upward projectile |
| Nuke | Clears active enemies |
| Shield | Absorbs one attack collision |

## Wave progression

A normal run contains 42 entries selected from 47 authored wave definitions.
The sequence combines opening waves, shuffled pools, an intermission and fixed
ending waves. The first three runs prepend the additional attack tutorial,
producing 43 entries.

Enemies begin in their entrance poses and roll into view from the horizon.

## DS presentation

The D-pad replaces the original tilt steering. The landscape playfield is scaled
to 256 pixels wide, with status and touch controls on the second screen.
Menus use the original art and font with a DS layout; scores are stored locally.

Simulation uses 1/60-second steps driven by the DS refresh interrupt. The current
software renderer presents roughly 32 FPS in melonDS. Animation curves and paths
are sampled for the DS runtime. Some enemy child transforms, particles, camera
shake and beam effects use simplified rendering.

Audio uses 22.05 kHz PCM: stereo music streams from NitroFS, while mono effects
are cached and mixed during play. No separate audio files are required beside
the ROM.

## Saving

A writable FAT/DLDI filesystem enables `fat:/coolpizza.stats`, which stores the
best score and play count. Without writable storage, these values remain in
memory until the game is closed. Play count determines the extra tutorial on
the first three runs.
