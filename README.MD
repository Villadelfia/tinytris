# Tinytris
A minimal implementation of a TGM-like tetris game, with some modern amenities.

- 180° IRS is possible by holding A and C during ARE.
- The rotation system can be switched between TGM2 and TGM3 mode.
- The RNG mode can be switched between TGM1, 2, and 3 modes.
- A hard drop button as well as a sonic drop button are provided.
- Automatic pause on focus loss and short delay before restarting.

Intended to be played while doing other things.

## Controls
Controls can be rebound in `config.ini`. These are the defaults:

- `A/D`: Left/right
- `W`: Sonic drop/up
- `S`: Soft drop/down
- `U`: Hard drop
- `J/L`: Rotate CCW
- `K`: Rotate CW
- `M`: Hold
- `R`: Restart game
- `Enter`: Begin game
- `ESC`: Quit game
- `-`: Scale down
- `=`: Scale up
- `P`: Mute/unmute
- `F1`: Toggle display of details pane
- `F2`: Toggle transparency of the field

Game controllers are supported as well. The title screen will display the default bindings as applied to the connected first controller.

## Replacing graphics
The main graphics can be replaced by providing a file named `atlas.png` in a `data` directory alongside the game exe.

An example of this file is included in the assets directory.

## Replacing sounds
All of the built-in sounds can be overridden by providing specific ogg files in a `data` directory alongside the game exe:

- `lineclear.ogg`: Played when you clear a line.
- `linecollapse.ogg`: Played when the cleared lines collapse down.
- `pieceland.ogg`: Played when a piece hits the stack.
- `piecelock.ogg`: Played when a piece locks.
- `irs.ogg`: Played when a piece spawns with initial rotation applied.
- `ready.ogg`: Played at the start of the new game countdown.
- `go.ogg`: Played at the end of the new game countdown.
- `hold.ogg`: Played when holding a piece.
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
- `move.ogg`: Played when moving in the menus and used as an indicator in extra feedback mode.

## Music
Music is supported. Songs and sections can be reconfigured in `config.ini`.

## Other settings
Several other settings can be adjusted from their default values in `config.ini`.

## Speed timings
The speed curves are defined in `timings*.ini`.

You can alter these, or create more to add modes.

## Details Pane
The details pane shows your section splits for the current game. It will remain after you top out and only clear when you start a new game.

In addition to the splits, it also shows:

- Level: Your current level, using TGM1/2 calculations.
- Gravity: Amount of cells a piece moves down every frame.
- Lockdel: The delay between a piece touching the ground and locking in place.
- DAS: How many frames you need to keep the left or right button held to automatically keep moving in that direction.
- ARE: Time in frames between one piece locking and the next one spawning.
- LN-ARE: Time in frames for the ARE when line was just cleared.
- Clear: Time in frames between the lines cleared disappearing, and the stack compacting down.
- Mode: A special string that can be used to identify the settings used to play that game.
- Hash: A unique hash identifying the speed curve selected. The name is not taking into account, only the values.

The mode string has the following elements, in this order:

- `Nk`: Which kicks are used. `1k` for TGM1/2 rotation, `3k` for TGM3 rotation.
- `Nr`: Which RNG is used. `1r`, `2r`, and `3r` stand for their respective TGM games.
- `Np`: How many previews were visible. `0p` to `3p`.
- `Xh`: If hold was enabled. `Nh` for no, `Yh` for yes.
- `XXg`: Gravity mode. `--g` for regular gravity, `20g` for forced 20g.
- `XXXX`: Piece visibility. `----` for regular visibility, `fade` for fading pieces, `invi` for invisible pieces.