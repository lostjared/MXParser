# SDL3 graphics module

This optional demo exports a small C API that can be imported through
`graphics.ls-h`. It uses one window, a persistent canvas, and integer image IDs,
so no SDL pointers or structs cross the interpreter's native-call boundary.

For a playable example without a demo timeout, see [Tetris in ls-C](tetris.md).
It includes colored blocks, keyboard input, line clearing, next/hold previews,
pause, and restart. Build and play with the `run-tetris` target.

[Columns in ls-C](columns.md) is another playable example: move and cycle
three-gem columns, match colors in any direction, and build cascade chains.
Launch it with `run-columns`; it also has no demo timeout.

## Build

Install SDL3 development files and libffi, then run from the repository root:

```bash
cmake -S . -B build/graphics -DWITH_SDL3_DEMO=ON
cmake --build build/graphics --target ls-c ls-graphics -j
```

The module is `build/graphics/libls-graphics.so` on Linux,
`build/graphics/libls-graphics.dylib` on macOS, or `ls-graphics.dll` in the
configuration directory on Windows. The module requires SDL 3.2 or newer.
BMP loading works with SDL 3.2; [PNG loading requires SDL 3.4](https://wiki.libsdl.org/SDL3/SDL_LoadPNG).
It does not require SDL_image or SDL_ttf. `WITH_SDL3_DEMO` defaults to `OFF`.

## Run a normal program

Run from the repository root so the sample image's relative path resolves:

```bash
./build/graphics/ls-c --run-program examples/graphics/demo.c \
    --module ./build/graphics/libls-graphics.so \
    --imports examples/graphics/graphics.ls-h
```

The demo draws an image, colored shapes, points, and text. Use A/D or click to
move the blue rectangle. Escape or the window's close button ends the demo;
it also exits after 600 frames (about ten seconds). The header's prototypes are
automatically available to the program; no `#include` is needed.

The same API works in a C++ host through
`interpreter.import_module(library_path, "examples/graphics/graphics.ls-h")`,
followed by the usual `load()` and `call()`.

## Use the REPL

Start `./build/graphics/ls-c --repl`, then enter:

```text
:import "./build/graphics/libls-graphics.so" "examples/graphics/graphics.ls-h"
gfx_open("REPL graphics", 640, 400)
gfx_color(18, 24, 38, 255);
gfx_clear();
gfx_color(255, 80, 100, 255);
gfx_fill_rect(40, 40, 160, 100);
gfx_color(255, 255, 255, 255);
gfx_point(20, 20);
gfx_rect(32, 32, 176, 116);
gfx_text(40, 160, "Hello from ls-C");
int image = gfx_image_load("examples/graphics/checker.bmp");
gfx_image_draw(image, 300, 80, 128, 128);
gfx_present();
```

Drawing commands update the canvas. `gfx_present()` displays it, and earlier
drawings remain available across presentations. Call `gfx_clear()` to erase it
using the current color. To keep the window responsive, poll events regularly.
For example, enter this loop, followed by a blank line to submit it:

```c
int running = 1;
while (running) {
    int event;
    while ((event = gfx_poll()) > 0) {
        if (event == 1 || (event == 2 && gfx_key() == 27))
            running = 0;
    }
    gfx_present();
    gfx_delay(16);
}

```

Then call `gfx_close();`. Ctrl+C also interrupts the interpreted loop; the window
stays available until `gfx_close()`, `:reset`, or interpreter destruction. SDL
events are processed by `gfx_poll()` on the calling thread, so the window does not
process input while the REPL is idle at a prompt. All graphics calls belong on
the main thread, as required by [SDL's renderer API](https://wiki.libsdl.org/SDL3/SDL_SetRenderTarget).

You can also preload the full demo and call it interactively:

```bash
./build/graphics/ls-c --repl examples/graphics/demo.c \
    --module ./build/graphics/libls-graphics.so \
    --imports examples/graphics/graphics.ls-h
```

Enter `graphics_demo(600, 16)` at the prompt.

## API

| Functions | Behavior |
| --- | --- |
| `gfx_open(title, width, height)`, `gfx_close()` | Open or close the single window. Opening another window releases the old canvas and images. |
| `gfx_color(r, g, b, a)`, `gfx_clear()` | Set the drawing color (each channel 0–255), or clear the canvas. Alpha blending applies to drawing. |
| `gfx_point(x, y)`, `gfx_line(x1, y1, x2, y2)` | Draw a point or line with the current color. Coordinates are pixels. |
| `gfx_rect(x, y, w, h)`, `gfx_fill_rect(x, y, w, h)` | Draw an outline or filled rectangle. Sizes must be nonnegative. |
| `gfx_text(x, y, text)` | Draw SDL's small built-in debug font with the current color. |
| `gfx_present()` | Copy the persistent canvas to the window and present it. |
| `gfx_image_load(filename)` | Load a BMP or PNG texture and return a positive image ID; 0 means failure. |
| `gfx_image_draw(id, x, y, w, h)` | Draw the image, scaling to the given size. Zero width/height uses the image's original width/height. |
| `gfx_image_width(id)`, `gfx_image_height(id)` | Query the image's original dimensions. |
| `gfx_image_free(id)` | Release an image. Freed IDs remain invalid, including after closing and reopening the window. |
| `gfx_poll()`, `gfx_running()` | Read the next supported event, or query whether the window is open without a quit request. |
| `gfx_key()`, `gfx_key_down(key)` | Read the last keyboard event's SDL keycode, or query a key's held state after polling. Printable keys use character codes; Escape is 27. |
| `gfx_key_repeat()` | Return 1 if the last keyboard event was an OS auto-repeat event, otherwise 0. |
| `gfx_mouse_x()`, `gfx_mouse_y()`, `gfx_mouse_button()` | Read data from the last mouse event. Button 1 is left, 2 middle, 3 right. |
| `gfx_delay(milliseconds)` | Sleep between frames. Delay must be nonnegative. |
| `gfx_ticks()` | Return monotonic elapsed milliseconds as an unsigned long long. |
| `gfx_message(title, message)` | Display a modal informational message box. |

Drawing/window operations return 1 on success and 0 on failure, with diagnostics
on stderr. `gfx_close()` returns void. Image IDs, dimensions, event accessors, and
state queries return the values described above. `gfx_poll()` returns -1 if no
window is open, and otherwise:

| Code | Event |
| --- | --- |
| 0 | No supported event in this poll cycle |
| 1 | Quit or window close request (`gfx_running()` becomes 0) |
| 2 | Key down |
| 3 | Key up |
| 4 | Mouse motion |
| 5 | Mouse button down |
| 6 | Mouse button up |

Poll until 0 each frame. Event getters retain their values until updated by
another event. Closing the window releases every image. Module teardown also
cleans up its resources, without shutting down other users of SDL's video
subsystem. The module owns one shared window per loaded library, so interpreters
that import the same library share that window.

## Validation

```bash
cmake --build build/graphics -j
ctest --test-dir build/graphics -R sdl3-graphics --output-on-failure
```

These tests use SDL's dummy video driver and software renderer. They exercise
the actual `.ls-h` import path in both the REPL and normal interpreter, check
rendered colors and images, inject keyboard/mouse/quit events, and verify cleanup.
The C++ test also runs two frames of `demo.c` and saves a rendered bitmap beneath
the build directory for inspection.
