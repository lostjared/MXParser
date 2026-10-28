# MXParser language reference

MXParser implements a restricted, single-translation-unit C99 language with an
interpreter, an interactive REPL, built-in formatted output, and optional native
function imports. This guide describes the language accepted by semantic
validation and executed by `mx::Interpreter` / `ls-c --run-program`.

Syntax parsing alone accepts some constructs that validation rejects. The older
`Evaluate`, `ICode`, and `HTMLCode` visitors support a smaller numeric subset;
their limitations do not describe the validated interpreter.

## Contents

- [Running source code](#running-source-code)
- [Source text and literals](#source-text-and-literals)
- [Types and numeric conversions](#types-and-numeric-conversions)
- [Declarations, scopes, and storage](#declarations-scopes-and-storage)
- [Expressions and operators](#expressions-and-operators)
- [Functions](#functions)
- [Statements and control flow](#statements-and-control-flow)
- [Arrays and pointers](#arrays-and-pointers)
- [Structs and enums](#structs-and-enums)
- [Initialization](#initialization)
- [Built-in formatted output](#built-in-formatted-output)
- [Native functions and imports](#native-functions-and-imports)
- [Interactive execution](#interactive-execution)
- [Diagnostics and execution limits](#diagnostics-and-execution-limits)
- [Unsupported features](#unsupported-features)
- [Parser and embedding APIs](#parser-and-embedding-apis)

## Running source code

After building as described in [README.md](README.md), use the `ls-c` executable
in your build directory. The following commands assume `build/ls-c`:

```sh
./build/ls-c --syntax-program program.c  # Parse and print the translation unit
./build/ls-c --check-program program.c   # Validate names and types without running
./build/ls-c --run-program program.c     # Validate, load globals, and call main
./build/ls-c --print-result program.c    # Also print Result: <return value>
./build/ls-c --repl                     # Start an interactive session
```

For example, save this as `program.c`:

```c
int square(int value) {
    return value * value;
}

int main(void) {
    int total = 0;
    for (int i = 1; i <= 3; ++i) {
        total += square(i);
    }
    lsprintf("total=%d\n", total);
    return total;
}
```

`--run-program` prints `total=14`; `--print-result` also prints `Result: 14`.
These modes write no intermediate-code or HTML files. A positional filename
without a mode selects the legacy evaluator and its ICode/`out.html` output.

## Source text and literals

Source uses C-style identifiers, case-sensitive names, whitespace, `//` line
comments, and `/* ... */` block comments. Statements and declarations normally
end with `;`. Braces group statements and initializer elements. Source file
extensions are not significant to the parser.

There is no preprocessor. Do not use `#include`, `#define`, conditional
compilation, or preprocessor line markers. Supply needed declarations directly
or load native import declarations through the import interface. C translation
features such as line splicing and trigraph replacement are outside the subset.

| Literal kind | Examples | Meaning |
| --- | --- | --- |
| Decimal integer | `0`, `42`, `1000L`, `7ULL` | Type selected from the permitted integer types by value and suffix |
| Octal/hexadecimal integer | `077`, `0xff`, `0X10u` | Nondecimal candidates can include unsigned types |
| Binary integer extension | `0b1010`, `0B11u` | Base-two integers accepted by the lexer/type model |
| Floating point | `2.5`, `1e3`, `0.5f`, `2.0L` | Unsuffixed is `double`; `f/F` selects `float`, `l/L` selects `long double` |
| Narrow character | `'A'`, `'\n'`, `'\x41'` | An integer character value of type `int` |
| Narrow string | `"hello"`, `"a" "b"` | Character array with a terminating zero; adjacent strings concatenate |

Integer suffixes use `u/U`, `l/L`, and `ll/LL` combinations. Decimal unsuffixed
integers try signed `int`, `long`, then `long long`; nondecimal literals also try
the corresponding unsigned types. Literals that exceed the permitted types fail
validation.

Character and string escapes include `\a \b \f \n \r \t \v \\ \' \" \?`,
up to three octal digits, and `\x` followed by hexadecimal digits. Escape values
must fit an 8-bit character. Hex escapes consume following hex digits: use
`"\x41" "B"` to separate the escape from a following `B`. Embedded zero bytes
are supported. Runtime character literals must decode to one character;
multicharacter constants are unsupported. Wide and UTF-prefixed literals are
outside the subset.

String literal storage lasts for the loaded program and is read-only. A local
pointer may point to it, but writing through that pointer fails at runtime.
An initialized character array instead contains its own copy.

## Types and numeric conversions

The interpreter uses a fixed LP64 model, including on hosts with another C ABI.
A byte has eight bits, plain `char` is signed, and signed integers use two's
complement representation.

| Type | Size in bytes | Representation |
| --- | --- | --- |
| `_Bool` | 1 | Zero or one |
| `char`, `signed char`, `unsigned char` | 1 | 8-bit integer |
| `short`, `unsigned short` | 2 | 16-bit integer |
| `int`, `unsigned int`, enum | 4 | 32-bit integer |
| `long`, `unsigned long` | 8 | 64-bit integer |
| `long long`, `unsigned long long` | 8 | 64-bit integer |
| `float` | 4 | Binary32 |
| `double`, `long double` | 8 | Binary64 |
| Object pointer | 8 | Checked interpreter object reference |

Legal C scalar keyword combinations such as `signed short int` and
`unsigned long int` are accepted. `void` is allowed for function results,
pointer targets, casts, and the sole no-parameter marker; it is not an object
type. `sizeof` produces the model's unsigned 64-bit size type.

Small integer and enum operands undergo integer promotion. Mixed arithmetic
uses the usual arithmetic conversions; assignments, arguments, and returns
convert to the destination type. Thus `5 / 2` is `2`, while `5 / 2.0` is `2.5`.
Floating-to-integer conversion truncates toward zero and rejects out-of-range
values. `float` operations and conversions round through binary32; `long double`
does not provide extra precision over `double` here.

Unsigned arithmetic wraps at the result type's width. Signed overflow, division
by zero, and invalid shifts fail at runtime. Signed narrowing keeps the target
width's two's-complement low bits; signed right shift is arithmetic. Converting
to `_Bool` maps zero/null to zero and other scalar values to one.

### Qualifiers

`const` and `volatile` can qualify objects and pointer levels. `restrict` can
qualify pointers to object types. Qualifiers affect type compatibility and
modifiable-lvalue checks:

```c
int value = 3;
const int *view = &value;  // The pointed-to value cannot be changed through view
int *const fixed = &value; // The pointer cannot be reassigned
*fixed = 4;
```

Pointer conversions respect pointed-to qualifiers. `volatile` does not expose
host memory or hardware access, and the runtime does not enforce all `restrict`
aliasing rules.

## Declarations, scopes, and storage

Declarations require a type. Multiple declarators share base specifiers but
have their own pointer, array, or function modifiers:

```c
int value = 1, *pointer = &value, samples[3] = {2, 3, 4};
typedef unsigned long Count;
Count count = 3;
```

File scope, function parameters, nested blocks, and `for` declarations provide
lexical scopes. A declaration becomes visible at its declarator, not at the
start of the block. Inner objects may shadow outer names. Duplicate objects in
one local scope and incompatible redeclarations fail validation.

Typedefs create scoped type aliases and share the ordinary identifier namespace
with objects, functions, and enumerators. Struct/enum tags have a separate
namespace; each struct has its own member namespace.

Ordinary local objects have automatic storage and expire when their scope exits.
Uninitialized automatic scalar reads fail at runtime. File-scope objects have
static storage and start zero-initialized. Globals retain values between calls
in the same interpreter/session.

Supported storage-class specifiers are:

| Specifier | Supported use |
| --- | --- |
| `typedef` | Scoped type aliases |
| `static` | File-scope objects and functions with internal linkage |
| `extern` | File-scope declarations with external linkage |

Compatible repeated file-scope declarations and tentative definitions are
supported within the loaded translation unit. `extern` does not automatically
resolve a host symbol: an object needs a definition, and an external function
needs a definition or an explicit native binding before execution uses it.
Block-scope `static`/`extern`, explicit `auto`/`register`, and `inline` are excluded.
Block-scope typed function prototypes are supported without storage specifiers.

## Expressions and operators

The table lists precedence from highest to lowest. Operators on one row have
equal precedence.

| Operators | Purpose | Associativity |
| --- | --- | --- |
| `()`, `[]`, `.`, `->`, postfix `++ --` | Call, subscript, member access, post-increment/decrement | Left |
| Prefix `++ --`, unary `+ - ! ~ * &`, `sizeof`, casts | Unary operations and type conversion | Right |
| `* / %` | Multiply, divide, remainder | Left |
| `+ -` | Addition, subtraction, supported pointer arithmetic | Left |
| `<< >>` | Integer shifts | Left |
| `< <= > >=` | Ordering | Left |
| `== !=` | Equality | Left |
| `&` | Bitwise AND | Left |
| `^` | Bitwise XOR | Left |
| `|` | Bitwise OR | Left |
| `&&` | Logical AND | Left |
| `||` | Logical OR | Left |
| `?:` | Conditional selection | Right |
| `= += -= *= /= %= <<= >>= &= ^= |=` | Assignment | Right |
| `,` | Evaluate left, then return right | Left |

Parentheses override precedence. Remainder, bitwise operations, and shifts
require integer operands. Comparisons and logical operations produce integer
truth values. `&&` and `||` short-circuit, and `?:` evaluates only its selected
arm. Conditions accept arithmetic values and pointers.

Assignment and increment/decrement require modifiable lvalues. Prefix increment
returns the updated value; postfix returns the old value. Compound assignment
evaluates its target once, so `samples[i++] += 2` advances `i` once.

Casts use `(type) expression`, including scalar types, typedefs, object pointers,
and permitted abstract declarators. Numeric conversions and compatible
object-pointer operations are supported; arbitrary integer/address conversion is
not. Casting to `void` discards a value.

`sizeof expression` and `sizeof(type)` use validated types and do not evaluate
the operand. For example, `sizeof samples` measures the whole array and
`sizeof(i++)` leaves `i` unchanged. The type must be complete and have a supported
object size; `void` and functions have no object size.

Integer constant expressions are used for array bounds, enumerators, and case
labels. They may use integer/character literals, prior enum constants, permitted
operators, integer casts, and `sizeof` complete types. Variables (including
`const int` objects), calls, assignments, increments, and comma operators do not
qualify as integer constant expressions.

## Functions

Functions have typed prototypes and fixed parameter lists. Definitions require
named parameters; prototypes may omit parameter names. Use `(void)` for no
parameters: legacy empty `()` is parsed, but rejected by validated execution.

```c
double average(double left, double right);

double average(double left, double right) {
    return (left + right) / 2.0;
}

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

A call requires a visible compatible prototype or definition. Put a prototype
before a call to a later definition. Calls check argument count and conversions;
recursion and separate local call scopes are supported.

Numeric values, object pointers, and structs can be parameters and return values.
Structs pass by value. Array parameters adjust to pointers; arrays themselves
cannot be returned or assigned. Functions cannot return functions, and function
pointers and user-defined variadic functions are excluded.

`return expression;` converts the result to the declared return type.
`return;` is for void functions. Reaching the end of `main` produces zero;
reaching the end of another non-void function fails when executed. A void result
cannot be used as an object value. File execution invokes `main` without
arguments; use `int main(void)` for command-line examples.

## Statements and control flow

| Form | Behavior |
| --- | --- |
| `{ ... }` | Block containing declarations and statements, with local scope |
| `;` | Empty statement |
| `expression;` | Evaluate an expression for its side effects |
| `if (condition) statement else statement` | Choose a branch; `else` is optional and binds to the nearest unmatched `if` |
| `while (condition) statement` | Test before each iteration |
| `do statement while (condition);` | Execute once before the first test |
| `for (init; condition; update) statement` | Initialize once, test, execute, then update |
| `switch (expression) statement` | Dispatch by an integer value to a case or default label |
| `break;` | Leave the nearest enclosing loop or switch |
| `continue;` | Continue the nearest loop, running the `for` update when applicable |
| `return [expression];` | Leave the current function |

A `for` initializer can be a declaration or expression; any of its expression
slots can be omitted. An omitted test is true. Variables declared in the
initializer are scoped to the loop.

Switch controls evaluate once. Case labels require integer constant expressions
and must remain distinct after conversion to the promoted control type. A switch
can have at most one default label. Stacked labels and fallthrough are supported:

```c
switch (value) {
case 1:
case 2:
    total += value;
    break;
default:
    total = 0;
}
```

Labels can occur inside nested statements of the switch. A dispatch that skips
a local initializer does not initialize that automatic object; reading it can
fail. `continue` inside a switch still targets an enclosing loop, while `break`
targets the switch if it is the nearest loop/switch construct. Invalid jump or
label contexts fail validation. Ordinary labels and `goto` are unsupported.

## Arrays and pointers

Fixed-size arrays, multidimensional arrays, arrays of pointers, pointers to
arrays, and parenthesized declarators are supported:

```c
int samples[3] = {10, 20, 30};
int matrix[2][2] = {{1, 2}, {3, 4}};
int (*row)[2] = &matrix[0];
int *cursor = samples;
cursor[1] = 25;
int last = *(cursor + 2);
```

Bounds must be positive integer constant expressions. An omitted outer bound
can be inferred from an initializer, appear in an adjusted array parameter, or
appear in a file-scope `extern` array completed by another declaration. Variable
length and flexible arrays are unsupported.

Arrays decay to pointers in ordinary value contexts, but retain array type for
`sizeof` and address-of. `&` takes an object's address, `*` dereferences a pointer,
and `p[i]` accesses an element. Pointer arithmetic advances by element size.
Compatible object pointers and `void *` can round-trip without losing identity;
integer constant zero supplies a null pointer.

Pointers carry allocation identity, offset, originating array/subobject bounds,
and lifetime information. Forming a one-past pointer is allowed; dereferencing
it is not. Arithmetic cannot escape its bounds, and subtraction requires
pointers into the same array. Null dereferences, expired local references,
read-only writes, and incompatible representation accesses fail explicitly.

This is typed object storage. Casting a pointer does not grant byte-level access
to another type's representation or widen a member pointer to its enclosing
struct. Arbitrary host addresses, raw memory allocation APIs, and general C
byte-aliasing behavior are outside the execution model.

## Structs and enums

Named and anonymous struct definitions, named forward declarations, nested
members, and self-referential pointers are supported:

```c
struct Node;
struct Node {
    int value;
    int samples[2];
    struct Node *next;
};

enum Mode { IDLE, ACTIVE = 4, DONE, };
```

Use `object.member` for a struct object and `pointer->member` for a struct
pointer. Member lookup is checked statically. Struct layout uses natural
alignment capped at eight bytes, member padding, and trailing padding.
Objects and by-value uses require complete types; pointers may refer to forward
declared structs. A struct cannot contain itself by value.

Struct assignment, value parameters, and returns copy all stored subobjects,
including array members. Pointer members retain pointers to the original
objects. Aggregate copy initialization is excluded; initialize with braces and
then assign when a copy is needed:

```c
struct Node original = {7, {1, 2}, 0};
struct Node copy = {0};
copy = original;
copy.samples[0] = 9; // original.samples[0] remains 1
```

Enums may be named or anonymous. The first implicit value is zero; subsequent
implicit values advance by one (`DONE` above is 5). Explicit values must be
integer constant expressions. Enumerators enter the ordinary identifier
namespace and can be used as bounds and case labels. Enum forward declarations
are excluded.

Unions, bit-fields, anonymous struct members, flexible array members, and empty
structs are unsupported.

## Initialization

Scalars use assignment expressions or a single braced value. Arrays and structs
use positional braces, with explicit braces for nested aggregate subobjects.
Trailing commas are allowed. Omitted aggregate elements are zero-filled.

```c
int count = 3;
int zeros[4] = {0};
int inferred[] = {1, 2, 3,};
int grid[2][2] = {{1}, {2, 3}};
char text[] = "hello";
char exact[3] = "abc"; // Fits the characters, with no terminating zero
```

String initialization copies characters into a character array and normally
includes the terminator; an explicit bound can omit just the terminator as shown.
The inferred bound includes it. Do not pass an unterminated array as an
unbounded `%s` string.

File-scope initializers require arithmetic constants or null pointers, including
inside aggregate initializers. Character-array string initialization is supported,
but global address constants such as `int *p = &global;` or
`const char *p = "text";` are excluded. Put such pointer initialization in a
function, or assign to a zero-initialized global during execution.

Local initializers can use runtime expressions and calls. Reading an automatic
scalar's own indeterminate value in its initializer fails. Designated
initializers, brace elision, empty initializer lists, compound literals, and
initialization of an aggregate from another aggregate expression are excluded.

## Built-in formatted output

`lsprintf` is available without a declaration or header and writes to application
stdout. Its name is reserved against source function declarations/definitions.
It returns an `int` byte count, or a negative value for an output-stream failure.

```c
int main(void) {
    int count = 3;
    double average = 2.5;
    lsprintf("count=%d, average=%.2f\n", count, average);
    return 0;
}
```

| Format feature | Supported forms |
| --- | --- |
| Integers | `%d %i %o %u %x %X` |
| Floating point | `%f %F %e %E %g %G %a %A` |
| Other conversions | `%c %s %p %n %%` |
| Flags | `- +` space `# 0` |
| Width and precision | Literal values or `*` arguments |
| Applicable length modifiers | `hh h l ll j z t L` |

Arguments undergo default integer/float promotions. Integer formats require
the expected promoted width: 32 bits for unmodified/`h`/`hh`, 64 bits for
`l`/`ll`/`j`/`z`/`t`. Floating formats require promoted `double`, or `long double`
for `L`. Use `%lu` for the model's `sizeof` result. Width/precision `*` arguments
must have the required integer type. Negative widths left-align, and negative
precisions are omitted.

`%s` and the format read checked character storage. A precision can bound `%s`
scanning of an unterminated array. `%c` can emit a zero byte. `%n` stores the byte
count through a writable pointer of the matching signed integer type. `%p` shows
a virtual interpreter address, not a host allocation address.

Invalid formats or argument types raise execution errors. Positional arguments,
wide `%lc`/`%ls`, and nonstandard host extensions are excluded. Output is buffered
until formatting succeeds; argument side effects and earlier `%n` writes remain
if a later conversion fails. Output/scanning limits apply per call. See
[examples/lsprintf.c](examples/lsprintf.c) for a complete example.

## Native functions and imports

Ordinary C library functions are not automatically available. The host can
register explicit C++ callbacks, or the optional native-module feature can bind
exported C functions from a shared library.

An import declaration file conventionally ends in `.ls-h`:

```c
__import__ int add(int left, int right);
__import__ double scale(double value, double factor);
__import__ int string_length(const char *text);
```

Load it together with its library:

```sh
./build/ls-c --run-program program.c \
    --module /path/to/library.so --imports /path/to/library.ls-h
```

The filename extension is arbitrary; choose the platform's library filename.
The header contains comments and import prototypes only, with no definitions,
typedefs, storage specifiers, or ordinary declarations. The library path comes
from the command/API, not from the header. Empty `()` import lists are normalized
to `(void)`, unlike ordinary validated source declarations.

Imported functions support numeric scalar parameters/results, void results,
and non-null `const char *` input strings. Values are range-checked against the
host ABI; string arguments are temporary copies valid for the call. Writable
pointers, pointer returns, structs, variadics, and native callbacks into
interpreted code are excluded at this shared-library boundary. Prototypes must
match exported C ABI functions. Execution budgets cannot preempt native code.

The CLI accepts the same paired import options for `--print-result` and
`--repl`. Inside the REPL, use `:import "library-path" "header-path"`.
Native imports require a build with `WITH_NATIVE_MODULES=ON` and libffi.
The optional [SDL3 examples](examples/graphics/README.md) use this mechanism;
graphics calls are imported APIs rather than built-in syntax.

Embedded C++ callbacks use `Interpreter::register_function` and a visible typed
C prototype. Their checked `NativeObject` views can inspect and modify
interpreter objects, including array/struct subobjects, for the callback's
duration. Callbacks return scalars or void and cannot reenter the same
interpreter. See the [embedding examples](README.md#calling-c-functions-from-interpreted-c)
for this separate interface and its lifetime rules.

## Interactive execution

Start `ls-c` without arguments or with `--repl`. Accepted variables, typedefs,
tags, functions, and side effects persist:

```text
>>> int count = 4
>>> count += 2
6
>>> double half(double x) { return x / 2.0; }
>>> half(count)
3
>>> lsprintf("count=%d\n", count);
count=6
```

Expressions display a bare result unless they end in `;`; void expressions have
no display. Declarations may omit their final semicolon. Statements, blocks,
and statement sequences execute without displaying an expression result.
Block and `for` declarations retain normal local scope.

Open a function or statement's `{` on its first line for multiline input.
Unclosed brackets, braces, parentheses, and block comments show the `...`
continuation prompt. Once continuation begins, enter a blank line after closing
the construct to submit it. This allows `else` and additional statements on
following lines. Blank lines inside unclosed constructs keep collecting input.

Interactive global initializers can call functions or use runtime expressions;
this is a REPL extension. Normal source files retain constant-initializer rules.
Earlier initializers and calls are never replayed. Duplicate-definition rules
still apply; assign to an existing variable rather than redeclaring it.

| Command/action | Effect |
| --- | --- |
| `:help` | Show REPL help |
| `:quit`, `:exit`, `exit()`, `quit()` | Exit the REPL |
| `:reset` | Clear session declarations and values |
| `:cancel` | Discard pending multiline input |
| `:import "library" "header"` | Load native function declarations/bindings |
| Ctrl-C | Cancel terminal input or interrupt interpreter execution |
| Ctrl-D on Unix | Exit at end of input |
| Ctrl-Z then Enter on Windows | Exit at end of input |

`--repl program.c` preloads declarations and initializers without calling
`main`. Compile/type errors leave accepted state unchanged. Runtime failures
retain completed writes and accepted declarations, so a failed initializer may
leave zero or partially initialized storage. Terminal editing/history is provided
by Readline on Unix and the console on Windows; redirected input has no prompts.
Session state and history are not saved across process exits.

## Diagnostics and execution limits

Syntax errors report the offending token, line, and column when available.
Semantic validation checks all source, including branches that never execute:
names, scopes, types, complete objects, conversions, calls, lvalues, initializer
shapes, constants, and valid jump contexts. Runtime errors include source
locations and detect failures that depend on execution.

| Default limit | Value |
| --- | --- |
| Instructions per load/call/submission | 20,000,000 |
| Recursion depth | 256 frames |
| Single object size | 1 MiB |
| `lsprintf` output/scanning size | 1 MiB per call |

Use `--instructions count` (`-n count`) to change the CLI budget. Zero disables
the instruction budget, useful for long-running graphics programs, while
preserving cancellation polling and other limits. Embedders configure
`ExecutionLimits` and can install an interrupt callback.

Runtime errors unwind active scopes and calls; completed global writes remain.
Checks include uninitialized scalar reads, integer overflow, zero division,
invalid shifts/conversions, invalid pointer access, and missing external
definitions. These checks do not diagnose every C undefined behavior, including
all unsequenced accesses, effective-type violations, or restrict violations.

## Unsupported features

The following are outside the validated language, even if an isolated syntax
fragment can be parsed:

- Preprocessor directives, includes, macros, multi-file linking, and automatic
  standard-library availability.
- Unions, bit-fields, anonymous struct members, flexible/variable-length arrays,
  enum forward declarations, and empty structs.
- Function pointers, indirect calls, old-style/unprototyped functions,
  user-defined variadics, and functions returning arrays or functions.
- Block-scope `static`/`extern`, explicit `auto`/`register`, and `inline`.
- Designated initializers, aggregate brace elision, empty initializer lists,
  aggregate copy initialization, compound literals, and global address constants.
- Ordinary labels and `goto`, wide/UTF literals, executable multicharacter
  constants, complex/imaginary/atomic types, `_Generic`, and `_Static_assert`.
- Raw host addresses, arbitrary pointer/integer casts, general byte-level
  representation aliasing, and built-in dynamic allocation.

Header-provided names such as `bool`, `true`, `false`, `NULL`, and `size_t` are
not automatically defined. Use `_Bool`, integer truth values, literal zero, or
explicit declarations/typedefs within the subset.

## Parser and embedding APIs

| API / mode | Responsibility |
| --- | --- |
| `Parser::parse_expression()` / `--syntax` | Expression AST syntax, without name/type/lvalue validation |
| `Parser::parse_expressions()` | Semicolon-separated expression ASTs; final semicolon optional |
| `Parser::parse_program()` / `--syntax-program` | Translation-unit AST and syntax printing |
| `SemanticAnalyzer` / `--check-program` | Static validation against the documented subset |
| `Interpreter::load()` and `call()` / `--run-program` | Validated execution using LP64 types and checked objects |
| `Interpreter::submit()` / `--repl` | Persistent interactive declarations, expressions, and statements |
| `Evaluate`, `Parser::parse()`, legacy filename mode | Older numeric execution; not the full validated interpreter |
| `ICode`, `HTMLCode` | Illustrative numeric/control-flow output, not an executable backend |

For expression-only parsing, `Parser::add_type_name()` registers a typedef name
to distinguish casts from parentheses. Translation-unit parsing tracks scoped
typedefs and ordinary-name shadowing directly. Syntax APIs consume their stream
without evaluating code or generating files.

Embedders must keep a loaded program AST alive. Public `Interpreter::call`
arguments/results use `ExecutionValue` (`monostate`, `int64_t`, `uint64_t`, or
`double`); pointer and struct passing remains available between interpreted C
functions. Use a C wrapper to expose scalar results at the public C++ boundary.

See [docs/c-subset.md](docs/c-subset.md) for grammar, AST/semantic API details,
and acceptance fixtures. [examples/subset.c](examples/subset.c) combines the main
features in one executable program.
