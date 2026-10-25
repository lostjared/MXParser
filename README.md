# MXParser

MXParser is a simple C++23 parser built on MXLex.

## Requirements

- A C++23 compiler
- Installed MXLex and Argz libraries/headers
- Pcons through [uv](https://docs.astral.sh/uv/), or CMake 3.10+
- GNU Readline development headers/library on Unix-like systems (`readline-devel`
  or `libreadline-dev`); Windows uses its native console API and needs no Readline

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
- `build/pcons/ls-c`

For example:

```bash
./build/pcons/parser-test --input /path/to/source-file
./build/pcons/ls-c /path/to/expression-file
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

## Interactive REPL

Start an interactive session with GNU Readline editing/history on Unix, or native
Windows console line editing on Windows:

```bash
./build/ls-c --repl
# Running ./build/ls-c without arguments also starts the REPL.
```

```text
>>> 1 + 2 * 3
7
>>> int count = 4
>>> count += 2
6
>>> double half(double x) {
...     return x / 2.0;
... }
>>> half(count)
3
>>> lsprintf("count=%d\n", count);
count=6
>>> :quit
```

Variables, typedefs, structs/enums, functions, and side effects persist. Declare
variables using C types; function definitions follow the existing C subset and
use `(void)` for no parameters. Open a function's `{` on its first line to enter
a multiline definition. Parentheses, brackets, braces, and block comments produce
`...` continuation prompts. Expressions print bare values (without `Result:`);
a trailing semicolon suppresses the value, and void calls print none. Declarations
can omit their final semicolon. Runtime initializers such as `double y=half(count)`
are allowed in the REPL. Accepted initializers and earlier calls are never replayed.

Use `:help`, `:quit`/`:exit`, `exit()`/`quit()`, `:reset` to clear the session, and
`:cancel` to discard unfinished input. Ctrl-C cancels pending terminal input;
Ctrl-D exits on Unix, and Ctrl-Z followed by Enter exits on Windows. Readline keeps
up to 500 input lines in memory for arrow-key history. The Windows console provides
native editing/history. Redirected input works on either platform without prompts.

Preload a file's declarations/functions without automatically calling `main`:

```bash
./build/ls-c --repl examples/functions.c
# Then enter: factorial(5), average(2.5, 5.0), or main()
```

Normal C duplicate-definition rules apply; use assignments to change variables
or `:reset` before defining them again. Compile/type errors leave accepted state
unchanged. Runtime errors report the problem and return to the prompt, retaining
completed writes; a validated declaration whose initializer fails remains bound
(with its zero-initialized or partially initialized storage). Object results show
an address or struct summary; use member expressions or `lsprintf` to inspect them.
Session state is discarded when the REPL exits, and history is not saved to disk.
Existing file execution/check/syntax modes keep their behavior.

The CMake Windows build does not search for or link Readline. For a Visual Studio
Debug build, start it with `.\build\Debug\ls-c.exe --repl`. Build with a C++23
Windows toolchain and the usual MXLex/Argz dependencies. Compiler-specific overflow
intrinsics have been replaced with portable checked arithmetic. On Unix, a
nonstandard Readline installation can be selected using `CMAKE_PREFIX_PATH`, or
`READLINE_INCLUDE_DIR` and `READLINE_LIBRARY`. Pcons also links Readline only on
non-Windows builds. The static parser library itself has no Readline dependency.

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
For translation units, `parse_program()` reads scoped typedef declarations
directly and tracks ordinary names that shadow them.

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
  registered typedef names, qualifiers, pointers, and array abstract declarators.

This is expression syntax parsing, without name resolution or type/lvalue
checking. Preprocessing, compound literals, `_Generic`, complex and atomic
type specifiers, and function-pointer declarations remain unsupported. Function
abstract declarators can be represented; their use still needs semantic validation.
Declarations and function bodies are supported through
`parse_program()`, described below.

Try the syntax parser from either build system:

```bash
./build/cmake/ls-c --syntax expressions.c
```

The `parse()` API and `ls-c filename` mode evaluate expressions and emit
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

Syntax parsing and semantic validation also support `do`/`while` and
`switch`/`case`/`default`, including stacked labels and fallthrough. Try
`ls-c --check-program examples/statements.c`. The validated `Interpreter` executes these statements, and ICode/HTML emit their
control flow. The legacy `Evaluate` visitor retains its original numeric subset.

For example, [examples/functions.c](examples/functions.c) combines a
floating-point average function with a recursive factorial function:

```bash
./build/cmake/ls-c examples/functions.c
# Prints intermediate code and writes out.html
./build/cmake/ls-c --syntax-program examples/functions.c
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

Use `ls-c --check-program filename` for name and type validation without
execution or output files. The library API is `mx::SemanticAnalyzer` in
`<MXParser/semantic.hpp>`; see the [semantic API and limits](docs/c-subset.md#semantic-validation).
Validation and `Interpreter` use the documented LP64 model; the legacy `Evaluate`
API retains its numeric subset and permissive behavior.

## Validated C subset

All eight roadmap steps are implemented for the documented single-translation-unit
subset, with execution limits listed below. [examples/subset.c](examples/subset.c) combines structs, enums, arrays,
initializers, pointer parameters, struct returns, and nested control flow:

```bash
./build/cmake/ls-c --check-program examples/subset.c
./build/cmake/ls-c --syntax-program examples/subset.c
```

The first command checks names/types; the second prints C syntax. Neither runs
source code nor writes `out.html`. To execute the same example with the validated
interpreter:

```bash
./build/cmake/ls-c --run-program examples/subset.c
# No output: this example returns 2 without calling lsprintf
```

This mode validates before execution and writes no HTML or intermediate code.
It prints only output requested by the program (such as `lsprintf` calls), with
no automatic `Result:` line. Use `ls-c --print-result filename` to explicitly
inspect the return value when debugging.
See the [exact boundary and API notes](docs/c-subset.md) for the fixed LP64 data
model and exclusions.

| Feature | Parsing | Semantic validation | Interpreter execution |
| --- | --- | --- | --- |
| Scalar declarations, typedefs, prototypes, functions | Yes | Names, scopes, qualifiers, conversions, and calls | Yes, LP64 arithmetic |
| Arithmetic, assignments, casts, calls, `?:`, comma | Yes | Operand and lvalue checks | Yes, LP64 arithmetic |
| Narrow characters and adjacent strings | Yes | Escape decoding and typed literals | Yes |
| Pointers, fixed arrays, subscripts, `sizeof` | Yes | Complete types, constant bounds, pointer conversions | Yes, with object bounds/lifetime checks |
| Structs, enum definitions, `.`, `->` | Yes | Scoped tags, layout, members, enum constants | Yes, including struct assignment/returns |
| Scalar, positional brace, string-array initializers | Yes | Type, shape, extents, and constant requirements | Yes, including omitted-element zero filling |
| Blocks, `if`, `while`, `for`, return and loop jumps | Yes | Conditions and jump contexts | Yes |
| `do`/`while`, `switch`, `case`, `default` | Yes | Conditions, constants, duplicate labels, contexts | Yes, including fallthrough and loop jumps |
| Built-in `lsprintf` | Direct calls without a declaration | Format pointer and scalar/pointer arguments | Formatted application stdout |
| Preprocessing, unions, bit-fields, VLA, function pointers, user-defined variadics, designated initializers, `goto` | Outside subset | Rejected with diagnostics | Unsupported |

CTest checks the final fixture contract, AST structure and printing, source
locations, malformed/EOF input, and visitor compatibility. When `cc`, `gcc`, or
`clang` is available, `subset-c99-comparison` also checks every positive fixture
and the combined example, both original and printed, with
`-std=c99 -pedantic-errors -fsyntax-only`. Set `MXPARSER_C99_COMPILER` at CMake
configuration time to select a compiler; configuration reports when this optional
comparison cannot be registered. The comparison does not link or execute C.

## Validated execution

`mx::Interpreter` in `<MXParser/interpreter.hpp>` validates and loads a program,
then calls its functions. Keep the AST alive while using it:

```cpp
std::istringstream input("unsigned int bump(unsigned int n) { return n + 1u; }");
mx::Parser parser(input);
auto program = parser.parse_program();
mx::Interpreter interpreter;
interpreter.load(*program);
auto result = interpreter.call("bump", {std::uint64_t{4294967295}});
std::cout << std::get<std::uint64_t>(result) << '\n'; // 0
```

`ExecutionValue` holds void (`monostate`), `int64_t`, `uint64_t`, or `double`.
Calls between C functions also pass and return pointers and structs; the public
C++ call boundary exchanges scalars. Use a C wrapper to expose a scalar result
from an object-valued function. Globals persist between calls. A successful
`load()` replaces the program; a failed load preserves the previous one.

The interpreter implements the fixed LP64 model, integer promotions, unsigned
wrapping, signed overflow checks, scalar conversions, and binary32 rounding for
`float`. `double` and `long double` both use binary64. It supports scoped objects,
arrays, strings, pointer references/arithmetic, positional initialization, struct
copies/parameters/returns, member access, unevaluated `sizeof`, and every parsed
control-flow statement. Compound assignments evaluate their target once.

Runtime checks report uninitialized reads, null/expired/one-past access, const
writes, invalid shifts, arithmetic overflow, and unresolved external objects or
functions. Pointer arithmetic stays within its originating array (including the
one-past address). This is typed object storage, not a byte-addressable host
process: incompatible representation casts and character-byte aliasing into
other object types, and casts that recover enclosing objects from member bounds,
are unsupported. External libraries, allocation functions,
and linker support remain outside the subset; `lsprintf` is the built-in stdout
exception. No complete undefined-behavior or
unsequenced-access detection is promised.

`ExecutionLimits` defaults to one million instructions per call/load, 256 call
frames, and one MiB per object. Interpreter failures throw `ExecutionException`
with source locations; validation failures throw `SemanticException`. Failed calls
clean up frames/scopes while retaining completed global writes.

`execution` tests runtime behavior and errors. When a system C compiler is
available, `execution-c99-comparison` builds and runs matching scalar-result
programs against the interpreter on an LP64 host. Fixed-model `sizeof(long double)`
and full-width public unsigned results have separate unit checks because host
storage/result conventions differ. Generated binaries and C files stay in the
build tree. Legacy expression mode and `Evaluate` retain their previous behavior;
ICode/HTML remain illustrative numeric output with explicit object-operation
errors, rather than a backend for the interpreter.

## Built-in formatted output

`lsprintf` writes printf-style formatted output to the application's stdout and
returns the number of bytes written, or a negative value on an output failure.
It is available automatically in the validated interpreter: no header or
prototype is needed.

```c
int main(void) {
    lsprintf("Hello, %s! count=%d, average=%.2f\n", "world", 3, 2.5);
    return 0;
}
```

```bash
./build/ls-c --run-program examples/lsprintf.c
```

Supported conversions are `%d`, `%i`, `%u`, `%o`, `%x`, `%X`, `%f`, `%F`, `%e`,
`%E`, `%g`, `%G`, `%a`, `%A`, `%c`, `%s`, `%p`, `%n`, and `%%`, with standard
flags, width/precision (including `*`), and applicable `hh`, `h`, `l`, `ll`, `j`,
`z`, `t`, and `L` modifiers. Arguments receive C default promotions. `%n` stores
its byte count through a correctly typed writable integer pointer. `%p` prints
an interpreter virtual address rather than a host-process address.

The format can be a string literal or a runtime character array/pointer. Missing
arguments, incompatible argument types, invalid strings/pointers, and unsupported
formats report execution errors. Wide `%lc`/`%ls`, POSIX positional arguments,
and nonstandard format extensions are outside this subset. The
`ExecutionLimits::output_bytes` budget defaults to one MiB per call and bounds
format/string scanning, field sizes, and generated output. `lsprintf` is a reserved
built-in function name and cannot be declared or defined by source programs;
ordinary local objects may shadow it under normal name lookup. This extension is
available through `Interpreter`/`--run-program`; legacy `Evaluate` is unchanged.

## Roadmap: basic C language subset

All steps below are implemented within their documented limits. The validated
`Interpreter` and the original numeric `Evaluate` API are both available.

The first target is a documented subset of C99 syntax for a single translation
unit: scalar types, typedefs, pointers, fixed-size arrays, simple structs and
enums, declarations, functions with prototypes, and structured control flow.
Source input must be free of preprocessing directives; a preprocessor, standard
library, linker, and native code backend are outside this milestone. Also defer
unions, bit-fields, variable-length arrays, function pointers, variadic functions,
compound literals, designated initializers, and `goto`/labels. This target does
not claim full C99 conformance.

Parsing, semantic validation, and execution are separate capabilities. The parser
produces the chosen subset's AST; the independent semantic pass checks names and
types. `Interpreter` executes validated programs; `Evaluate` remains the legacy
numeric subset described above.

1. **Define the grammar and acceptance cases.**
   Status: complete. See the [subset contract and support matrix](docs/c-subset.md)
   and [acceptance fixture manifest](tests/subset/manifest.tsv). The CTest
   `subset-fixtures` suite enforces final parsing and semantic expectations for
   every fixture, including named exclusion diagnostics.

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
   source locations with token lines and columns; see [the API notes](docs/c-subset.md#structured-types-and-source-locations).
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
   Status: complete. Declarations, parameters, casts, and `sizeof` now share
   declarator parsing. See the [declaration API notes](docs/c-subset.md#declarations-and-typedef-scopes)
   and [examples/declarations.c](examples/declarations.c). CTest covers type
   binding, scoped typedefs, array parameters, round trips, and excluded forms.

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
   Status: complete. Use `mx::SemanticAnalyzer::validate()` or
   `ls-c --check-program filename` to check a translation unit without
   execution. See the [semantic API and limits](docs/c-subset.md#semantic-validation).
   CTest checks the step 1–4 semantic fixtures and the scalar model, scopes,
   qualifiers, calls, returns, and diagnostics.

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
   Status: complete for parsing and validation. See the
   [structured-statement notes](docs/c-subset.md#structured-statements) and
   [examples/statements.c](examples/statements.c). CTest covers case conversion,
   label/jump contexts, nesting, round trips, and visitor compatibility.

   - Add `do`/`while`, `switch`, `case`, and `default` AST nodes and parsing.
     Retain existing blocks, empty statements, `if`/`else`, `while`, `for`,
     `return`, `break`, and `continue`.
   - Track loop and switch contexts separately: `break` can leave either, while
     `continue` targets the enclosing loop. Validate integral case constants,
     duplicate cases, and multiple defaults; preserve switch fallthrough.
   - Done when nested loop/switch examples and dangling-`else` cases produce
     the expected ASTs and invalid jump contexts are rejected.

6. **Add basic aggregate declarations and initialization.**
   Status: complete for parsing and validation. See the
   [aggregate notes](docs/c-subset.md#aggregates-and-initialization) and
   [examples/aggregates.c](examples/aggregates.c). CTest covers struct layout and
   member access, enum constants, array bounds, initializer shape, and round trips.
   Aggregate execution is available through step 8's validated `Interpreter`.

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
   Status: complete. See the [published matrix](#validated-c-subset),
   [combined example](examples/subset.c), and [verification notes](docs/c-subset.md#publication-and-verification).
   CTest audits every AST kind, accepted fixture round trips, malformed/EOF
   diagnostics, explicit visitor errors, and optional system C99 comparisons.

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
   Status: complete for the validated interpreter. See [execution support](#validated-execution)
   and [runtime details](docs/c-subset.md#validated-execution). Object operations
   execute in `Interpreter`; illustrative ICode/HTML support numeric do/switch
   control flow and explicitly reject unsupported object operations.

   - Align scalar evaluation with the documented data model, then implement
     object storage, lvalue references, pointers, arrays, strings, aggregate
     initialization, member access, and `sizeof`. Add execution for the new
     control-flow nodes and update intermediate-code visitors accordingly.
   - Verify numeric conversions, object lifetime, short-circuit behavior,
     fallthrough, and loop jumps with runtime tests. Compare defined results
     against compiled C examples and document interpreter limits.
   - Done when the support matrix identifies exactly which parsed constructs
     can execute and every remaining execution gap produces a clear error.
