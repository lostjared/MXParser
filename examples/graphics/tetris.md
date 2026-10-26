# Tetris in ls-C

`tetris.c` is a complete falling-block game written in the interpreted C subset.
The SDL3 module supplies drawing, keyboard input, and a millisecond clock; the
board, seven-piece bag, collisions, rotations, scoring, and game loop are ls-C.

## Play

Build and launch from the repository root:

```bash
cmake -S . -B build/graphics -DWITH_SDL3_DEMO=ON
cmake --build build/graphics --target run-tetris -j
```

The `run-tetris` target builds the interpreter and graphics module, then opens
the game. It passes `--instructions 0` to disable the instruction budget for this
session. There is no frame cap, session timer, or automatic exit at game over.
Restart with R, or quit with Escape/the window's close button.

To launch it directly after building:

```bash
./build/graphics/ls-c --run-program examples/graphics/tetris.c \
    --module ./build/graphics/libls-graphics.so \
    --imports examples/graphics/graphics.ls-h \
    --instructions 0
```

Use `.dylib` on macOS or the built `ls-graphics.dll` path on Windows. The game
does not use external images. See [the graphics guide](README.md) for SDL3 build
requirements.

## Controls

| Keys | Action |
| --- | --- |
| Left/right arrows or A/D | Move; holding repeats after a short delay |
| Down arrow or S | Soft drop |
| Up arrow, W, or X | Rotate clockwise |
| Z | Rotate counterclockwise |
| Space | Hard drop |
| C | Hold/swap the piece, once per lock |
| P | Pause/resume |
| R | Start a new game |
| Escape or window close | Quit |

Each seven-piece bag contains every tetromino once. The outlined ghost shows
where a hard drop will land. The sidebar shows the next and held pieces, score,
cleared lines, and level. Rotations try small wall/floor kicks. Clearing one,
two, three, or four rows awards 100/300/500/800 points multiplied by the current
level. Soft/hard drops add 1/2 points per cell. Gravity speeds up every ten lines.
Pause freezes gravity; game over waits for restart or quit.

The drawing loop repaints when the board or interface changes. It keeps polling
events while paused or at game over, and uses the native monotonic clock rather
than counting rendered frames for gravity and key repeat.

## REPL

```bash
./build/graphics/ls-c --repl examples/graphics/tetris.c \
    --module ./build/graphics/libls-graphics.so \
    --imports examples/graphics/graphics.ls-h \
    --instructions 0
```

Enter `main()` to play. After quitting, session globals remain available for
inspection, for example `tetris_score` and `tetris_board[19][0]`. Call `main()`
again for another game. Ctrl+C interrupts interpreted execution; use
`gfx_close();` to close the remaining window if you interrupt instead of quitting.

## Validation

```bash
cmake --build build/graphics -j
ctest --test-dir build/graphics -R 'sdl3-tetris|cli-arguments|repl-state' --output-on-failure
```

The Tetris test runs the actual ls-C source with the SDL3 module through the
interpreter. It checks bags, collisions, rotation kicks, line clears, scoring,
holds, top-out, gravity, pause, restart, and injected keyboard events. It saves a
rendered game fixture under `build/graphics/tetris-tests-data/` using SDL's dummy
video driver and software renderer.
