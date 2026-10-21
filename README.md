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

## C expressions

Use `Parser::parse_expression()` to obtain an expression AST without evaluating
it or writing output files. It accepts one expression and an optional trailing
semicolon, and throws `ParserException` for invalid input:

```cpp
#include <MXParser/parser.hpp>
#include <sstream>

std::istringstream input("ptr->items[i + 1] != 0 ? f(x, y) : -1");
mx::Parser parser(input);
auto expression = parser.parse_expression();
std::cout << expression->to_string() << '\n';
```

`parse_expressions()` returns a vector of ASTs for semicolon-separated
expressions. The final semicolon is optional; empty statements are ignored.
Both APIs consume their input stream. Register typedef names before parsing
with `parser.add_type_name("size_t")` to distinguish casts from parentheses.

Supported syntax includes:

- Arithmetic, shifts, comparisons, bitwise and logical operators with C
  precedence and associativity.
- Right-associative assignments and compound assignments, `?:`, and the comma
  operator.
- Unary `+ - ! ~ * &`, prefix/postfix `++ --`, calls, subscripts, and `.` / `->`
  member access.
- Integer, floating-point, character, and string literals, including adjacent
  string literals, as tokenized by MXLex.
- Casts and `sizeof` with scalar types, tagged `struct`/`union`/`enum` types,
  registered typedef names, qualifiers, and pointer types.

This is expression syntax parsing, without name resolution or type/lvalue
checking. Declarations, preprocessing, compound literals, `_Generic`, complex
and atomic type specifiers, and array/function abstract declarators in type
names are not implemented.

Try the syntax parser from either build system:

```bash
./build/cmake/parse-expr --syntax expressions.c
```

The existing `parse()` API and `parse-expr filename` mode still evaluate
expressions and emit intermediate code plus `out.html`. Their evaluator uses
signed 64-bit integers and supports integer operators, variable assignments,
increments, and short-circuit `&&`, `||`, and `?:`. Calls, pointers, member
access, casts, `sizeof`, and non-integer literals require a custom AST visitor;
the built-in visitors report unsupported operations. New visitor overloads
have throwing defaults so existing visitors remain source-compatible.

Run the expression regression tests with:

```bash
cmake -S . -B build/cmake -DBUILD_TESTING=ON
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```
