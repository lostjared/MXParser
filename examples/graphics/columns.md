# Columns in ls-C

`columns.c` is a playable falling-gem game written in the interpreted C subset.
The SDL3 module supplies graphics, keyboard input, and a clock. All board logic,
matching, gravity, cascades, scoring, and controls are ls-C.

## Play

```bash
cmake -S . -B build/graphics -DWITH_SDL3_DEMO=ON
cmake --build build/graphics --target run-columns -j
```

The target builds the interpreter and graphics module, then launches the game
with `--instructions 0`. There is no demo timeout or frame cap. Game over waits
for restart or quit.

To launch directly after building:

```bash
./build/graphics/ls-c --run-program examples/graphics/columns.c \
    --module ./build/graphics/libls-graphics.so \
    --imports examples/graphics/graphics.ls-h \
    --instructions 0
```

Use `.dylib` on macOS or your built `ls-graphics.dll` path on Windows. No image
assets are needed. See [the graphics guide](README.md) for SDL3 prerequisites.

## Rules and controls

Move a falling column of three gems and cycle their order to arrange matches of
three or more identical colors horizontally, vertically, or diagonally. Matched
gems highlight briefly, clear together, and let the remaining gems fall. Further
matches produce chain reactions. Overlapping matches count each gem once.

| Keys | Action |
| --- | --- |
| Left/right arrows or A/D | Move; holding a key repeats after a short delay |
| Up arrow, W, or X | Cycle gem order |
| Z | Cycle in the opposite direction |
| Down arrow or S | Soft drop |
| Space | Hard drop |
| P | Pause/resume, including during a cascade |
| R | Restart |
| Escape or window close | Quit |

The outlined ghost shows the landing position, and the sidebar previews the next
column. Six colors appear on a 6-by-13 board. Each cleared gem scores
`10 * level * chain`, with the chain starting at 1 for each placed column.
Soft/hard drops add 1/2 points per row. Every 30 cleared gems raises the level
and increases falling speed. The sidebar also shows cleared gems and the best
chain. A blocked spawn or a column locking partly above the board ends the game.

Movement and cycling stop while matches resolve. Pause freezes gravity and the
clear delay; input and rendering remain responsive at pause and game over.

## REPL

```bash
./build/graphics/ls-c --repl examples/graphics/columns.c \
    --module ./build/graphics/libls-graphics.so \
    --imports examples/graphics/graphics.ls-h \
    --instructions 0
```

Enter `main()` to play. After quitting, inspect `columns_score`, `columns_gems`,
`columns_best_chain`, or `columns_board[12][0]` at the prompt. Call `main()` again
to start a new game. If you interrupt with Ctrl+C, call `gfx_close();` to close
the remaining window.

## Validation

```bash
cmake --build build/graphics -j
ctest --test-dir build/graphics -R sdl3-columns --output-on-failure
```

The test loads the actual ls-C source and SDL3 import header, checks collisions,
cycling, match directions, crossing/long matches, stable gravity, chain scoring,
level progression, top-out, pause, restart, and injected keyboard input. It also
checks a rendered gem pixel and saves a game fixture beneath
`build/graphics/columns-tests-data/`. Rendering tests use SDL's dummy video driver
and software renderer.
