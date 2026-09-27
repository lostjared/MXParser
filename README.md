# MXParser

MXParser is a simple C++23 parser built on MXLex.

## Requirements

- A C++23 compiler
- Installed MXLex and Argz libraries/headers
- Pcons through [uv](https://docs.astral.sh/uv/), or CMake 3.10+

MXParser consumes installed dependencies and does not require their source
repositories to be adjacent to the MXParser checkout. Standard prefixes such
as `/usr` and `/usr/local` are searched automatically.

## Building with Pcons

After installing MXLex and Argz, run from the MXParser repository:

```bash
uvx pcons -B build/pcons --reconfigure
```

If either dependency is installed under a non-standard prefix, pass it through
`PREFIX`. Multiple prefixes use the platform path separator (`:` on Linux):

```bash
uvx pcons -B build/pcons \
    PREFIX=/path/to/mxlex-prefix:/path/to/argz-prefix \
    --reconfigure
```

This builds the static `mxparser` library and three example/test programs:

- `build/pcons/test-parser`
- `build/pcons/parser-test`
- `build/pcons/parse-expr`

For example:

```bash
./build/pcons/parser-test --input /path/to/source-file
./build/pcons/parse-expr /path/to/expression-file
```

Use `VARIANT=debug` for a debug build or `PROGRAMS=0` to build only the
library.

### Installation

The default installation is staged under `dist`:

```bash
uvx pcons -B build/pcons all install
```

To use another prefix:

```bash
uvx pcons -B build/pcons \
    PCONS_INSTALL_PREFIX=/path/to/prefix \
    PCONS_FINAL_PREFIX=/path/to/prefix \
    all install
```

The installation contains `libmxparser.a`, the public headers, and
`lib/pkgconfig/mxparser.pc`. Consumers must also be able to find the installed
MXLex package referenced by that metadata.

## Building with CMake

For dependencies installed in standard prefixes:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake
cmake --install build/cmake --prefix /path/to/prefix
```

For non-standard dependency prefixes:

```bash
cmake -S . -B build/cmake \
    -DCMAKE_PREFIX_PATH="/path/to/mxlex-prefix;/path/to/argz-prefix"
cmake --build build/cmake
```
