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
checking. Preprocessing, compound literals, `_Generic`, complex and atomic
type specifiers, and array/function abstract declarators in type names are not
implemented. Numeric declarations and function bodies are supported through
`parse_program()`, described below.

Try the syntax parser from either build system:

```bash
./build/cmake/parse-expr --syntax expressions.c
```

The `parse()` API and `parse-expr filename` mode evaluate expressions and emit
intermediate code plus `out.html`. For a file containing declarations and
functions, they load the program and call `main()` when it is defined.
New visitor overloads have throwing defaults so existing visitors remain
source-compatible.

## Functions and floating-point arithmetic

`parse_program()` returns a program AST containing function prototypes,
definitions, and global numeric declarations. Function bodies support local
numeric declarations (including multiple declarators), nested blocks, `return`,
`if`/`else`, `while`, `for`, `break`, `continue`, and expression statements.
Functions support typed parameters and return values, recursion, forward calls,
and separate local scopes. Use `(void)` or `()` for a function without parameters.

For example, [examples/functions.c](examples/functions.c) combines a
floating-point average function with a recursive factorial function:

```bash
./build/cmake/parse-expr examples/functions.c
# Result: 21, followed by intermediate code; also writes out.html
./build/cmake/parse-expr --syntax-program examples/functions.c
```

To call a parsed function directly:

```cpp
std::istringstream input("double half(double x) { return x / 2.0; }");
mx::Parser parser(input);
auto program = parser.parse_program();
mx::Evaluate evaluator;
program->accept(evaluator);
evaluator.call("half", {5.0});
std::cout << evaluator.result() << '\n'; // 2.5
```

Keep the program AST alive while using its functions. `clear()` clears the
result stack while preserving variables and registered functions. `result()`
now returns `long double` to expose fractional results; `result_value()` returns
a `NumericValue` variant holding `int64_t`, `double`, or `monostate` for void.

The evaluator retains integer arithmetic for integer operands (`5 / 2 == 2`)
and promotes mixed arithmetic to floating point (`5 / 2.0 == 2.5`). It supports
floating assignments, compound assignments, increments, comparisons, unary
operators, numeric casts, and short-circuit `&&`, `||`, and `?:`. Remainder,
bitwise operators, and shifts require integer operands. Numeric parameters,
returns, and assignments convert to their declared type; float conversions
round through C++ `float`. Integer overflow, division by zero, invalid calls,
and use of uninitialized locals report runtime errors.

This is a numeric C interpreter subset. Integer types use signed 64-bit storage
without C's distinct integer widths or unsigned arithmetic. Floating arithmetic
uses `double`, including `long double` declarations and `L` literals; this is
not a complete implementation of C's type system. Character/string evaluation,
pointer operations, `sizeof` evaluation, arrays, structs, function pointers,
variadic functions, external/library calls, preprocessing, `switch`, `do`/`while`,
and `goto` remain unsupported. Function recursion is limited to 256 frames.
The emitted intermediate code is illustrative and is not a native C backend.

Run the expression and function regression tests with:

```bash
cmake -S . -B build/cmake -DBUILD_TESTING=ON
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```
