# Tinytris

A minimal implementation of a TGM2-like tetris game, with some modern amenities.

- 180° IRS is possible by holding A and C during ARE.
- The rotation system can be switched between TGM2 and TGM3 mode.
- A hard drop button as well as a sonic drop button are provided.
- Infinite speed curve, with the first 999 levels matching TAP Master mode.
- Automatic pause on focus loss and short delay before restarting.

Intended to be played while doing other things.

## Controls

Controls can be rebound in `config.ini`. Defaults will be used if this file is absent.

- `A/D`: Left/right
- `W`: Sonic drop
- `S`: Soft drop
- `U`: Hard drop
- `J/L`: Rotate CCW
- `K`: Rotate CW
- `R`: Restart game
- `Enter`: Begin game
- `ESC`: Quit game
- `0`: Toggle between Classic ARS and Ti ARS, shown as `[TAP]` and `[Ti ]` respectively
- `-`: Scale down
- `=`: Scale up
- `P`: Mute/unmute
- `T`: Toggle transparency of the field
- `F1`: Toggle display of detailed timer splits

## Replacing graphics
The main skin for the blocks can be replaced by providing a file named `block.png` in a `data` directory alongside the game exe.

This image should contain a 4 wide by 2 high grid of square images. These images represent the shapes:

```text
XILO
ZTJS
```

X here being the skin used when you top out.

## Replacing sounds
All of the built-in sounds can be overridden by providing specific ogg files in a `data` directory alongside the game exe:

- `lineclear.ogg`: Played when you clear a line.
- `linecollapse.ogg`: Played when the cleared lines collapse down.
- `pieceland.ogg`: Played when a piece hits the stack.
- `piecelock.ogg`: Played when a piece locks.
- `irs.ogg`: Played when a piece spawns with initial rotation applied.
- `ready.ogg`: Played at the start of the new game countdown.
- `go.ogg`: Played at the end of the new game countdown.

In addition, several sounds are supported but are not included by default:

- `i_mino.ogg`: Played when a piece spawns and the next mino is an I piece.
- `s_mino.ogg`: Played when a piece spawns and the next mino is an S piece.
- `z_mino.ogg`: Played when a piece spawns and the next mino is a Z piece.
- `j_mino.ogg`: Played when a piece spawns and the next mino is a J piece.
- `l_mino.ogg`: Played when a piece spawns and the next mino is a L piece.
- `o_mino.ogg`: Played when a piece spawns and the next mino is an O piece.
- `t_mino.ogg`: Played when a piece spawns and the next mino is a T piece.
- `section_lock.ogg`: Played once if the level counter gets stuck at a section break.
- `section_pass.ogg`: Played when the level counter rolls over a new section.
- `tetris.ogg`: Played when clearing 4 lines at once.
- `tetris_b2b.ogg`: Played when clearing a tetris right after clearing another tetris.
- `combo.ogg`: Played when clearing 2 or more lines after clearing one or more line.
- `gameover.ogg`: Played when topping out.
- `complete.ogg`: Played when completing a game.

## Music
Music is supported. The game will look for the following songs in the data directory by default:

- `menu_h.ogg`: Intro of the menu song.
- `menu_b.ogg`: Looping part of the menu song.
- `section_0_h.ogg`: Intro of the song played from 0-280.
- `section_0_b.ogg`: Looping part of the song played from 0-280.
- `section_1_h.ogg`: Intro of the song played from 300-480.
- `section_1_b.ogg`: Looping part of the song played from 300-480.
- `section_2_h.ogg`: Intro of the song played from 500-680.
- `section_2_b.ogg`: Looping part of the song played from 500-680.
- `section_3_h.ogg`: Intro of the song played from 700-980.
- `section_3_b.ogg`: Looping part of the song played from 700-980.
- `section_4_h.ogg`: Intro of the song played from 1000+.
- `section_4_b.ogg`: Looping part of the song played from 1000+.

Songs and sections can be reconfigured in `config.ini`.

## Other settings

Several other settings can be adjusted from their default values in `config.ini`.

## Speed timings

The speed curves are defined in `timings*.ini`.

You can alter these, or create more to add modes.

## Features

You just play. If you need to pause, just click away and the game will pause.