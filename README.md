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

## Roadmap: basic C language subset

The following steps are planned work, building on the expression parser and
numeric function interpreter described above. Complete them in order, keeping
the existing expression and program APIs working throughout.

The first target is a documented subset of C99 syntax for a single translation
unit: scalar types, typedefs, pointers, fixed-size arrays, simple structs and
enums, declarations, functions with prototypes, and structured control flow.
Source input must be free of preprocessing directives; a preprocessor, standard
library, linker, and native code backend are outside this milestone. Also defer
unions, bit-fields, variable-length arrays, function pointers, variadic functions,
compound literals, designated initializers, and `goto`/labels. This target does
not claim full C99 conformance.

Parsing, semantic validation, and execution are separate capabilities. The parser
should produce an AST for the entire chosen subset; a semantic pass should check
names and types before execution. The evaluator can remain a numeric subset
until the final optional step. Document support separately for each capability.

1. **Define the grammar and acceptance cases.**
   Status: complete. See the [subset contract and support matrix](docs/c-subset.md)
   and [acceptance fixture manifest](tests/subset/manifest.tsv). The CTest
   `subset-fixtures` suite checks current parsing expectations; target parsing
   and semantic expectations remain explicit work for later milestones.

   - Write a compact grammar and feature matrix covering the target above,
     current support, planned additions, and explicit exclusions. Specify the
     accepted storage classes, qualifiers, initializer forms, and declarator
     restrictions so that "basic C" has a testable boundary.
   - Preserve the existing expression and function tests as the baseline. Add
     small valid and invalid source fixtures for each upcoming milestone.
   - Done when every target feature has an acceptance example and unsupported
     constructs have an expected diagnostic, with no implied runtime support.

2. **Introduce structured types and source locations.**
   Status: complete. Parsed types now use the shared model in
   [MXParser/type.hpp](MXParser/type.hpp). AST nodes and parser exceptions carry
   source locations; see [the API notes](docs/c-subset.md#structured-types-and-source-locations).
   CTest covers type structure, lexer coverage, and diagnostic locations.

   - Replace type strings as the internal representation with scalar, qualified,
     pointer, array, function, and tagged types. Keep printable type names for
     AST output and compatibility where needed.
   - Preserve source locations from MXLex on AST nodes and report the offending
     token, line, and expected syntax in parser errors. Check lexer coverage
     for the chosen keywords, literals, and punctuation before extending grammar.
   - Done when existing casts, `sizeof`, declarations, and function signatures
     use the shared type model and malformed input reports useful locations.

3. **Unify declaration and declarator parsing.**
   - Parse declaration specifiers separately from each declarator, including
     pointer qualifiers, fixed-size array suffixes, function prototypes, and
     parenthesized declarators within the chosen subset. Reuse the machinery
     for cast and `sizeof` type names, including abstract declarators.
   - Support scoped `typedef` declarations and ordinary-name shadowing, building
     on `add_type_name()` for expression-only callers. Give each declarator its
     own derived type, so `int *p, count;` declares distinct types.
   - Done when globals, locals, parameters, prototypes, and definitions share
     the same declaration rules; tests distinguish `int *items[4]` from
     `int (*items)[4]` and reject deferred function-pointer declarations clearly.

4. **Add name resolution and semantic validation.**
   - Extend the symbol-table infrastructure with translation-unit, function,
     and block scopes, plus the separate tag namespace. Resolve variables,
     functions, typedef names, and compatible repeated declarations.
   - Check operand types, modifiable lvalues, calls, return values, casts,
     qualifiers, and loop/jump context without executing the AST. Define a
     scalar data model for sizes, signedness, promotions, and conversions.
   - Done when undeclared names, incompatible prototypes, invalid assignments,
     and invalid returns are diagnosed even inside branches that never execute.
     Keep any permissive legacy expression-evaluation behavior explicit.

5. **Complete structured statements.**
   - Add `do`/`while`, `switch`, `case`, and `default` AST nodes and parsing.
     Retain existing blocks, empty statements, `if`/`else`, `while`, `for`,
     `return`, `break`, and `continue`.
   - Track loop and switch contexts separately: `break` can leave either, while
     `continue` targets the enclosing loop. Validate integral case constants,
     duplicate cases, and multiple defaults; preserve switch fallthrough.
   - Done when nested loop/switch examples and dangling-`else` cases produce
     the expected ASTs and invalid jump contexts are rejected.

6. **Add basic aggregate declarations and initialization.**
   - Parse simple struct and enum definitions, tagged forward declarations,
     fixed-size arrays, scalar initializers, positional brace initializers,
     and character-array string initializers. Decode character/string escapes
     and evaluate the integer constant expressions needed for enums and bounds.
   - Resolve member access and validate array bounds, initializer shape, complete
     types, and illegal recursive objects; allow self-references through pointers.
   - Done when a small program can declare an array of structs, access its
     members, and use enum constants, with invalid layouts and initializers
     covered by rejection tests. Execution can still be unsupported.

7. **Validate and publish the parser subset.**
   - Extend AST printing and visitor interfaces for every new node. Each visitor
     must either handle the node or report an explicit unsupported operation.
   - Add end-to-end syntax examples combining declarations, functions, arrays,
     structs, and control flow. Check AST structure, parse/print/parse stability,
     malformed input, and unexpected end-of-file errors through CTest.
   - Compare acceptance of supported fixtures with a system C compiler in C99
     syntax-only mode; exclude extensions and undefined behavior from fixtures.
     Update the README feature matrix and examples to match verified support.
   - Done when all target grammar features and semantic checks pass their
     acceptance cases, existing regressions pass, and exclusions fail clearly.

8. **Optionally expand execution after the parser milestone.**
   - Align scalar evaluation with the documented data model, then implement
     object storage, lvalue references, pointers, arrays, strings, aggregate
     initialization, member access, and `sizeof`. Add execution for the new
     control-flow nodes and update intermediate-code visitors accordingly.
   - Verify numeric conversions, object lifetime, short-circuit behavior,
     fallthrough, and loop jumps with runtime tests. Compare defined results
     against compiled C examples and document interpreter limits.
   - Done when the support matrix identifies exactly which parsed constructs
     can execute and every remaining execution gap produces a clear error.
