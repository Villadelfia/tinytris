# Tinytris

A minimal implementation of a TGM2-like tetris game, with some modern amenities.

- 180° IRS is possible by holding A and C during ARE.
- The rotation system can be switched between TGM2 and TGM3 mode.
- A hard drop button as well as a sonic drop button are provided.
- Infinite speed curve, with the first 999 levels matching TAP Master mode.
- Automatic pause on focus loss and short delay before restarting.

Intended to be played while doing other things.

## Controls

Controls are hardcoded, but easy enough to adjust in the source file.

- `A/D`: Left/right
- `W`: Sonic drop
- `S`: Soft drop
- `U`: Hard drop
- `J/L`: Rotate CCW
- `K`: Rotate CW
- `R`: Restart game
- `Enter`: Begin game
- `ESC`: Quit game
- `0`: Toggle between Classic ARS and Ti ARS, shown as [TAP] and [Ti ] respectively
- `-`: Scale down
- `=`: Scale up
- `P`: Mute/unmute

## Features

You just play. If you need to pause, just click away and the game will pause. There are intentionally basically no features.