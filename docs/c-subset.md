# Basic C subset contract

This is the implemented acceptance contract for README roadmap steps 1–8.
It describes a restricted C99 subset, not full C99 conformance or execution
support for every accepted program.
The [fixture manifest](../tests/subset/manifest.tsv) maps examples to milestones.

## Capability matrix

"Partial" means the restrictions below matter. `SemanticAnalyzer` now provides
an independent validation pass for the implemented grammar, including code that
never executes. Steps 1–7 implement and audit the grammar and validation described below. Step 8 adds the validated `Interpreter`; the legacy `Evaluate` visitor remains a numeric subset.

| Feature | Current parsing | Current validation / execution | Target parsing / validation | Step | Acceptance fixtures |
| --- | --- | --- | --- | --- | --- |
| Arithmetic, assignments, calls, comma, `?:`, casts | Yes | Static operand/lvalue/conversion checks; numeric execution | Implemented for current types | 2, 4 | `expressions`, `invalid-lvalue`, `bad-operands` |
| Integer, floating, character, adjacent string literals | Yes | Typed numeric facts and decoded narrow literals; numeric execution | Yes / typed, decoded literals | 2, 6 | `literals`, `scalars` |
| Scalar types and qualifiers | Structured scalar/qualified types | LP64 validation and qualifier checks; validated interpreter uses LP64 arithmetic; legacy Evaluate uses signed 64-bit storage/double | Implemented for current types | 2, 4 | `scalars`, `bad-type`, `const-write`, `bad-restrict` |
| Numeric globals, locals, multiple declarators, block scopes | Yes; independent derived types | Static lexical resolution and redeclaration checks; numeric execution | Implemented | 3, 4 | `numeric-program`, `mixed-declarators`, `duplicate-local` |
| Prototypes, definitions, typed parameters, recursion, void | Yes | Static compatibility, arity, argument and return checks; direct numeric calls execute | Implemented for typed prototypes | 3, 4 | `numeric-program`, `prototype-conflict`, `bad-call`, `bad-return` |
| Scoped typedefs and ordinary-name shadowing | Yes; expression API can also register type names | Numeric aliases execute; parser tracks declaration visibility | Yes / scoped typedef disambiguation implemented | 3 | `typedef-scope`, `bad-typedef` |
| Pointers, arrays, parenthesized and abstract declarators | Yes within the chosen restrictions; array parameters adjust to pointers | Typed pointer/array storage and execution; constant positive bounds validated | Yes / bound and complete-type checks | 3, 6 | `declarators`, `abstract-types`, `array-parameter`, `bad-declarator` |
| `sizeof`, address/dereference, `.`, `->`, subscripting | Yes | Static pointer/complete-type checks, member lookup, and LP64 layout | Yes / validated and executable member access | 4, 6 | `expressions`, `abstract-types`, `aggregates`, `unknown-member` |
| Blocks, empty statements, `if`/`else`, `while`, `for`, jumps | Yes | Static scalar-condition and jump-context checks; numeric execution | Implemented for existing statements | 4, 5 | `numeric-program`, `dangling-else`, `bad-break`, `bad-continue` |
| `do`/`while`, `switch`, `case`, `default`, fallthrough | Yes; label order and nesting preserved | Static condition, constant-case, duplicate/default and context checks; validated execution and illustrative numeric ICode | Implemented for existing integer types | 5 | `structured-statements`, `bad-case`, `duplicate-case`, `duplicate-default`, `switch-continue` |
| Struct definitions and forward declarations | Named/anonymous definitions and tagged forward declarations | Separate scoped tags, layout, completeness, members, and object execution | Yes / recursive objects rejected | 4, 6 | `aggregates`, `incomplete-object`, `recursive-object` |
| Enum definitions and integer constant expressions | Named/anonymous definitions and tag references | Enum values, integer constant expressions, positive bounds, and execution | Yes / constant values and positive array bounds | 6 | `aggregates`, `bad-bound`, `nonconstant-bound` |
| Scalar, positional brace, and character-array string initializers | Yes, with explicit aggregate braces | Recursive shape/type checks, inferred extents, narrow escapes, and initialization | Yes / type, shape, and bounds checking | 6 | `initializers`, `bad-initializer` |
| File-scope `static` and `extern` | Preserved in AST and printing | Static storage/linkage conflict checks and compatible repeated declarations | Implemented within one translation unit; no linking | 3, 4 | `storage`, `bad-storage` |
| AST source locations and useful diagnostics | AST start locations and parser exception locations | Line, column when available, offending token, expected syntax; lexer reason retained | Implemented for current grammar | 2 | `missing-semicolon`, `unexpected-eof` |
| Combined translation unit and AST round trip | Yes | Syntax/type checks, AST round trips, C99 comparisons, and validated execution | Verified within the chosen subset | 7 | `combined` |
| Excluded features listed below | Some forms parse before semantic rejection | Named exclusion diagnostics; no execution | Verified rejection at the applicable phase | 1, 7 | All `excluded-*` fixtures |

Fixture names in this document omit `.c`. Every negative fixture has an expected
diagnostic meaning in the manifest. Those meanings are a future contract, not
assertions about today's exact exception text.

## Exact boundary

- Input is one translation unit of C99 tokens, with comments and ordinary narrow
  literals. No `#` directives, include processing, macros, line splicing, or
  trigraph translation is provided. Preprocessed input must have line markers
  removed. Reject directives explicitly rather than ignoring them.
- Scalar spellings include `void`, `_Bool`, signed/unsigned `char`, `short`,
  `int`, `long`, `long long`, `float`, `double`, and `long double`, using legal
  C combinations. `void` is only a return type, pointer target, cast target, or
  sole parameter-list marker, never an object type.
- The planned semantic data model has 8-bit bytes; 8/16/32/64-bit integer
  widths for char/short/int/long, with long long also 64 bits, two's complement
  signed integers, signed plain char,
  a one-byte `_Bool`, binary32 float, and binary64 double/long double.
  Pointers and `sizeof` results use 64 bits. Implement integer promotions,
  usual arithmetic conversions, and assignment conversions against that model.
  This describes future validation, not the current evaluator's storage model.
- Accept `const` and `volatile` on objects and pointer levels; `restrict` only
  qualifies pointers to object types. Preserve qualifiers in the AST; validate
  const writes and pointer qualification conversions. Volatile accesses and
  restrict aliasing do not gain execution semantics in the parser milestone.
- Storage classes are limited to scoped `typedef`, file-scope `static`, and
  file-scope `extern`. Ordinary block objects have automatic storage. Defer
  block-scope `static`/`extern`, explicit `auto`/`register`, and `inline`.
  Diagnose conflicting storage classes. Support compatible file-scope
  redeclarations and tentative definitions within this unit; no external linking.
- Each declarator owns its pointer, array, and function modifiers. Accept
  pointers to arrays, arrays of pointers, and functions returning object pointers.
  Reject function pointers, functions returning arrays/functions, arrays of
  functions, and arrays with variable length. Fixed bounds must be positive
  integer constant expressions. Allow omitted bounds when an initializer
  determines the size, in an array parameter adjusted to a pointer, or in a
  file-scope `extern` array declaration completed by another declaration.
- User functions have typed prototypes and non-variadic parameter lists; definitions
  require named parameters. Use `(void)` for zero parameters. Keep existing
  empty `()` parsing for compatibility, but unprototyped function declarations
  and old-style definitions are outside the validated target. A void call is
  valid as a statement; it cannot supply an object value. Forward calls require
  a visible prototype in validated input, even though the interpreter currently
  permits calls to later definitions.
- Typedef names share the ordinary identifier namespace. Tags occupy a separate
  namespace, and each struct has its own member namespace. Allow block shadowing;
  reject duplicate local objects and incompatible declarations in one scope.
  Ordinary declaration visibility starts at its declarator, not at block entry.
- Support named and anonymous simple struct/enum definitions, named struct
  forward declarations, nested struct members, enum constants with optional
  integer constant values, and trailing enum commas. No enum forward declaration,
  anonymous struct members, flexible array members, bit-fields, or unions.
  Layout uses the documented scalar sizes, natural alignment capped at eight
  bytes, member padding, and trailing padding. Objects require complete types;
  pointer self-references are valid. Struct assignment and value parameters and
  returns are included in semantic validation and `Interpreter` execution.
- Initializers are scalar assignment expressions, recursively nested positional
  braces for arrays/structs, and narrow strings for character arrays. Allow
  trailing commas, zero-fill omitted aggregate elements, and infer omitted outer
  array bounds. Defer brace elision, designated initializers, compound literals,
  and aggregate initialization from another aggregate expression. File-scope
  initializers require arithmetic constants or null pointers in this first
  subset; defer address constants. Local struct assignment remains valid.
- Integer constant expressions for enums, bounds, and cases can use integer and
  character constants, prior enum constants, integer casts, permitted operators,
  and `sizeof` complete types. Reject variable references, calls, assignments,
  increments, invalid arithmetic, and comma operators in these contexts.
- Support all expression operators already documented in the README. Validate
  operands, calls, modifiable lvalues, members, and pointer arithmetic. `sizeof`
  checks its operand's type without evaluating it. Compound assignment evaluates
  its left operand once. No function-pointer calls or pointer/integer casts
  other than null-pointer conversions in the initial semantic subset.
- Conditions require scalar values; switch expressions require integer types.
  `break` exits a loop or switch; `continue` requires an enclosing loop, including
  when nested in a switch. Case values must be distinct after conversion to the
  promoted switch type. Reject duplicate defaults. Preserve fallthrough.
- Also exclude wide/UTF literals, complex/imaginary and atomic types, C11 generic
  selection and assertions, labels/`goto`, and compiler-specific extensions.
  Reject excluded constructs with a diagnostic naming the unsupported feature.
  Existing expression-only parsing of a union tag or other fragments can remain;
  it does not imply support in the validated translation-unit subset.

## Compact target grammar

EBNF: `{ X }` means repetition, `[ X ]` means optional, and quoted text is a
token. `identifier`, `literal`, and `scalar-specifiers` are lexical categories;
the rules above constrain combinations and contexts. This is the target grammar,
not a transcription of the current recursive-descent implementation.

```text
translation-unit = { external-declaration } ;
external-declaration = declaration | function-definition ;
function-definition = declaration-specifiers declarator compound-statement ;
declaration = declaration-specifiers [ init-declarator
              { "," init-declarator } ] ";" ;
declaration-specifiers = declaration-specifier { declaration-specifier } ;
declaration-specifier = storage-class | qualifier | type-specifier ;
storage-class = "typedef" | "static" | "extern" ;
qualifier = "const" | "volatile" | "restrict" ;
type-specifier = scalar-specifiers | typedef-name | struct-specifier
                 | enum-specifier ;
struct-specifier = "struct" ( identifier [ struct-body ] | struct-body ) ;
struct-body = "{" member-declaration { member-declaration } "}" ;
member-declaration = declaration-specifiers declarator { "," declarator } ";" ;
enum-specifier = "enum" ( identifier [ enum-body ] | enum-body ) ;
enum-body = "{" enumerator { "," enumerator } [ "," ] "}" ;
enumerator = identifier [ "=" constant-expression ] ;
init-declarator = declarator [ "=" initializer ] ;
declarator = { "*" { qualifier } } direct-declarator ;
direct-declarator = ( identifier | "(" declarator ")" ) { suffix } ;
suffix = "[" [ constant-expression ] "]" | "(" [ parameter-list ] ")" ;
parameter-list = "void" | parameter { "," parameter } ;
parameter = declaration-specifiers [ declarator | abstract-declarator ] ;
type-name = declaration-specifiers [ abstract-declarator ] ;
abstract-declarator = pointer-part [ abstract-direct ] | abstract-direct ;
pointer-part = "*" { qualifier } { "*" { qualifier } } ;
abstract-direct = "(" abstract-declarator ")" { suffix } | suffix { suffix } ;
initializer = assignment-expression
            | "{" initializer { "," initializer } [ "," ] "}" ;
compound-statement = "{" { declaration | statement } "}" ;
statement = compound-statement | [ expression ] ";"
          | "if" "(" expression ")" statement [ "else" statement ]
          | "while" "(" expression ")" statement
          | "do" statement "while" "(" expression ")" ";"
          | "for" "(" ( declaration | [ expression ] ";" )
                     [ expression ] ";" [ expression ] ")" statement
          | "switch" "(" expression ")" statement
          | "case" constant-expression ":" statement
          | "default" ":" statement
          | "return" [ expression ] ";" | "break" ";" | "continue" ";" ;
expression = assignment-expression { "," assignment-expression } ;
assignment-expression = conditional-expression
                      | unary-expression assignment-op assignment-expression ;
conditional-expression = binary-expression
                         [ "?" expression ":" conditional-expression ] ;
binary-expression = cast-expression { binary-op cast-expression } ;
cast-expression = "(" type-name ")" cast-expression | unary-expression ;
unary-expression = postfix-expression | ( "++" | "--" ) unary-expression
                 | ( "+" | "-" | "!" | "~" | "*" | "&" ) cast-expression
                 | "sizeof" ( unary-expression | "(" type-name ")" ) ;
postfix-expression = primary { "[" expression "]"
                    | "(" [ assignment-expression
                            { "," assignment-expression } ] ")"
                    | ( "." | "->" ) identifier | "++" | "--" } ;
primary = identifier | literal | "(" expression ")" ;
constant-expression = conditional-expression ;
```

Binary precedence, low to high: `||`, `&&`, `|`, `^`, `&`, `== !=`,
`< <= > >=`, `<< >>`, `+ -`, `* / %`. These operators associate left.
Assignments (`= += -= *= /= %= <<= >>= &= ^= |=`) and `?:` associate right;
comma associates left. Adjacent string literal tokens form one primary.
An `else` binds to the nearest unmatched `if`. Specifiers must include exactly
one base type (possibly a combination of scalar keywords). Semantic constraints exclude
illegal declarator forms and type/storage combinations admitted by this compact
grammar. A declaration without a declarator must declare a struct/enum tag or
definition; it cannot be a bare scalar or typedef alias use.

## Acceptance and diagnostics

Run the existing CMake/CTest commands in the README. The `subset-fixtures` test
reads every manifest entry and checks current syntax behavior through
`parse_program()` without evaluation or output files. Current and target parsing
must agree; every successfully parsed fixture also checks its semantic expectation
through `SemanticAnalyzer`, including step 7 exclusions. A semantic rejection can
therefore be a syntax acceptance. Excluded fixtures additionally assert diagnostic
keywords naming their unsupported feature.
Parse errors must produce a
nonempty `ParserException` with a line, token, and column when available; exact wording is not frozen. Other exceptions fail
the test. The existing expression and function regressions remain unchanged.

The manifest records target parsing, target validation, and the expected
diagnostic meaning independently. As milestones land, update current parsing
expectations and add semantic/AST assertions to the corresponding test suite.
Do not change a target rejection into acceptance just to match current behavior.
Step 2 adds source-location assertions in `types-and-source`; step 3 verifies
per-declarator types and scoped typedefs in `declarators`. Step 4 adds `semantics`
and check-only CLI tests. Step 5 adds `statements` and syntax/check-only statement
examples; step 6 extends checks to aggregates and complete constant-expression validation;
step 7 adds combined AST round trips and compiler comparisons.
Step 8 uses the same positive fixtures as candidates for runtime tests only
after documenting execution support. No fixture in this suite implies it can
currently be interpreted.

## Structured types and source locations

Roadmap step 2 is implemented for the existing grammar. `mx::TypePtr` is a shared
pointer to a `mx::Type` with immutable syntax fields and shared semantic tag layout state. Inspect `kind` before reading the corresponding
fields: `scalar` for scalar types; `qualifiers` and `element` for qualified types;
`element` for a pointer target or array element; and `element` plus `parameters`
for a function result and parameter types. `has_prototype` distinguishes typed
parameter lists (including `(void)`) from legacy empty `()` lists. Arrays have an optional `bound`.
Tagged types preserve `tag` and `name`; typedef references use `Named`. Aliases
parsed from declarations retain their definition in `element`; names supplied
through `add_type_name()` remain unresolved. `unqualified()` follows aliases and
removes outer qualifier wrappers without removing qualifiers from a pointer
target. `top_qualifiers()` includes qualifiers inherited through typedefs.
`equals()` compares structure, alias names/definitions, and qualifier placement,
independent of spelling; it does not perform scoped tag resolution or C type
compatibility checking. `to_string()` preserves parsed type-name spelling and
prints abstract declarators for types built using factories.

Parser-created `CastNode`, type-form `SizeofNode`, `DeclarationNode`, `Parameter`,
and `Declarator` objects expose `type`. A `FunctionNode::type` is a function type;
`result_type()` and `signature_type()` provide convenient access. Each declarator
now owns its complete derived type; `DeclarationNode::type` is its shared base
specifier type. Additional declaration syntax is described below; execution of
derived object types remains unsupported.

Existing public `type_name` and `return_type` strings are compatibility snapshots.
The evaluator and visitors use structured types when present; changing a snapshot
does not change an already parsed type. Legacy string constructors remain, and
manually built nodes without `type` resolve their legacy spelling on access.
The numeric evaluator keeps its documented integer widths and floating behavior;
the new representation does not implement C's full conversions or `sizeof`.

Every parser-created AST node, parameter, and declarator has a `location` holding
its starting token, 1-based line, and 1-based column. Compound expression locations refer to the
beginning of the expression, including an opening parenthesis when present.
Line zero denotes a manually constructed object with no source; column zero
denotes an unavailable column. Columns follow MXLex byte positions, with a tab
counting as one column. MXLex does not provide filenames or end positions.

Syntax errors expose `ParserException::location` and report expected syntax,
line, column when available, and the found token (`end of input` at EOF).
Lexical errors retain MXLex's reason and scanner line. If MXLex throws before returning a token, the diagnostic
uses `invalid lexical input` in place of an unavailable token spelling; a reason
such as an unterminated comment can also include its starting line. The current
lexer does not expose columns for EOF or scanner exceptions, so these remain
zero and diagnostics omit the column. Returned error tokens do retain columns. Legacy I/O
errors and invalid uses of token-access APIs have no source location.

```cpp
std::istringstream input("(const unsigned long *)p");
mx::Parser parser(input);
try {
    auto expression = parser.parse_expression();
    const auto &cast = dynamic_cast<const mx::CastNode &>(*expression);
    std::cout << cast.type->to_string() << '\n'; // const unsigned long *
    std::cout << cast.location.line << ':' << cast.location.column << '\n'; // 1:1
} catch (const mx::ParserException &error) {
    std::cerr << error.what() << '\n';
}
```

## Declarations and typedef scopes

Roadmap step 3 is implemented. A shared parser separates declaration specifiers
from pointer, array, and function modifiers. It handles grouped declarators,
unnamed prototype parameters, abstract declarators in casts and `sizeof`, and
multiple declarators with independent types. For example, `int *p, n;` has a
pointer and a scalar; `int *items[4];` has an array of pointers; and
`int (*items)[4];` has a pointer to an array. Functions may return pointers to
arrays, and declarations may mix object and function declarators. Top-level
prototypes remain `FunctionNode` objects; block prototypes are function-typed
declarators in a `DeclarationNode`.

`DeclarationNode::type` / `type_name` now describe the shared specifier type.
Use `declarators[i].type` for the complete type of an individual declaration.
`DeclarationNode::is_typedef` marks aliases, and `storage` on declaration/function
nodes preserves specifiers such as `typedef`, `static`, and `extern`. Legacy
manually constructed declarators without `type` still use the node's type.
`Type::declaration(name)` prints a complete named declaration without a semicolon;
`Type::declarator(name, base)` prints only the modifiers above a shared base.
Object declarations preserve grouping, including in `for` initializers. Mixed
top-level object/function declarations normalize into separate AST nodes, with
function prototypes retaining typed versus unspecified parameter lists.

Array types retain bound syntax as `bound_expression`, a shared expression AST.
For integer literal bounds, `bound` also contains the literal extent when it
fits `size_t`; other expressions are not folded yet. An omitted bound has
neither field. Parameters keep their original type in `declaration_type`, while
`type` and the function signature contain the adjusted pointer type for arrays.
This includes pointer-to-array types for multidimensional parameters. Bounds
still need the later checks for constant expressions, positive extents, complete
types, and excluded variable-length arrays: accepting their syntax is not
semantic approval. Bracket qualifiers and `static`/`*` array-parameter syntax
are not part of this milestone.

Typedef visibility follows translation-unit, prototype, function-body, nested
block, and `for` scopes. Ordinary names shadow outer typedefs, beginning after
their declarator and before their initializer. Parameters share the function
body's outer scope, while prototype-only parameter names disappear after the
parameter list. A `for` initializer's names disappear after its body. Alias
types retain their definition at the declaration point, even if the original
name is later shadowed. Typedef/ordinary-name conflicts needed for disambiguation
are diagnosed by parsing; the independent step 4 pass adds declaration
compatibility and name/type validation. Expression-only callers can still use `add_type_name()`.

Function-pointer declarations, function-pointer parameters (including implicit
adjustment from function parameters), and variadic lists report explicit
unsupported-feature errors. Arrays of functions and functions returning arrays
or functions are rejected. Aggregate definitions and brace initializers are described in the
[step 6 notes](#aggregates-and-initialization). Numeric aliases execute in the existing interpreter; typedef
declarations create no runtime objects. Pointer and array execution, and block
function-prototype execution, remain unsupported. No C integer-width, linkage,
or full compatibility semantics are implied by this parser milestone.

Try the syntax example with:

```bash
./build/cmake/ls-c --syntax-program examples/declarations.c
```

## Semantic validation

Step 4 adds an explicit, non-executing pass in
[`MXParser/semantic.hpp`](../MXParser/semantic.hpp). Parsing remains a syntax API;
call validation before evaluation when the stricter subset is required:

```cpp
#include <MXParser/parser.hpp>
#include <MXParser/semantic.hpp>
#include <sstream>

std::istringstream input("int twice(int n) { return n * 2; }");
mx::Parser parser(input);
auto program = parser.parse_program();
mx::SemanticAnalyzer analyzer;
analyzer.validate(*program); // throws SemanticException with line, column, token
```

From the CLI:

```bash
./build/cmake/ls-c --check-program examples/functions.c
./build/cmake/ls-c --check-program examples/declarations.c
```

This mode prints a success message or the first parser/semantic error and returns
nonzero on failure. It never calls `main`, evaluates source expressions, emits
intermediate code, or writes `out.html`. `parse()`, the default CLI evaluation
mode, and the standalone evaluator keep their existing permissive behavior;
validation is opt-in. For example, the evaluator accepts legacy empty `()`
parameter lists and forward calls without a preceding prototype, while the
validator requires `(void)` or typed parameters and a declaration visible at
the call site. A visible prototype may refer to an externally defined function;
validation performs no linking or external calls.

The semantic symbol table extends the existing symbol-table infrastructure with
typed ordinary identifiers and a separate tag namespace. Translation-unit,
prototype, function-body, nested-block, and `for` scopes follow declaration order;
parameters share the function body's outer scope. Duplicate locals, incompatible
prototypes, duplicate definitions, object/function conflicts, typedef conflicts,
and storage/linkage conflicts are diagnosed. Compatible file-scope declarations
and tentative definitions can repeat. Block function prototypes bind only inside
their scope while retaining compatibility with later declarations of the same
external function. Struct tags receive scoped identities; identical spellings
in distinct scopes do not make pointer types compatible. Typedefs retain their
definition-time types even across later shadowing.

All existing statements and expression branches are visited regardless of
runtime reachability. Checks cover modifiable lvalues and const writes; scalar
conditions; integer-only remainder/bitwise/shift operands; arithmetic and pointer
operations; array decay/subscripts; address/dereference; direct calls and argument
conversions; return types; casts; pointer qualification; `sizeof` complete object
types; and loop jumps. Pointer conversions allow object/void pointers and null
pointer constants, preserve pointed-to qualifiers, and reject unsafe nested
qualification changes such as `int **` to `const int **`. Pointer arithmetic needs
a complete target type. Function pointers, unsupported storage/type specifiers,
and member access on incomplete structs fail explicitly.

`ScalarDataModel` defines the LP64 sizes specified above, integer promotions,
usual arithmetic conversions, signedness, and floating ranks. It can report sizes
of scalars, pointers, complete structs/enums, and arrays with known bounds. The validator types
integer literals using their base, suffix, rank, and representable range; checks
floating literal range; types narrow character literals as `int`; and gives
narrow adjacent strings a character-array type including one trailing null.
Basic standard, octal, and hexadecimal escapes are recognized; wide/UTF and
universal-character escape handling remain unsupported in validated input.

After successful validation, `analyzer.info(expression)` returns its resolved
type, lvalue/modifiability flags, resolved symbol where applicable, and available
constant/null-pointer facts. Arithmetic operators and conditionals expose their
common result type. Integer constant facts use a `uint64_t` bit representation
at the expression's target width; `constant_expression` records arithmetic
constant syntax and does not certify every constant-value constraint. Folding
avoids signed overflow and division-by-zero and never executes source calls.
This metadata does not insert conversions into the AST. Keep the AST and analyzer
alive while reading facts; the next `validate()` resets prior results, including
after a failure. `symbols()` exposes the remaining translation-unit scope;
expression facts retain local-symbol references after local scopes close.

The legacy `Evaluate` visitor retains signed 64-bit integer storage and double
arithmetic. The validated `Interpreter` added in step 8 uses this LP64 model. Static type
approval therefore does not promise C-equivalent evaluation or pointer execution.
Step 6 extends this pass with aggregates, initialization, and constant positive
bounds as described below. `sizeof` validates unevaluated operand types and
reports complete aggregate sizes. This pass does not prove initialization,
all-path returns, reachability, runtime bounds, alias safety, or termination.

## Structured statements

Step 5 adds `DoWhileNode`, `SwitchNode`, `CaseNode`, and `DefaultNode`. Each
contains its parsed condition/value where applicable and its body or associated
statement. Labels are statements attached to the following statement; they are
not flattened into independent case blocks. This preserves stacked labels,
fallthrough, nested labels inside other statements, and the nearest-switch
association. Source locations retain the `do`, `switch`, `case`, or `default`
token's line and column. AST printing produces valid statement syntax and keeps
the original label order. Empty statements retain the existing empty-block AST
representation. Under this C99 subset, a label must precede a statement; use
braces or an empty statement before a declaration, and do not leave a label
immediately before a closing brace.

The parser tracks loop and switch nesting independently. `break` requires either
context; `continue` requires a loop even when written inside a switch. Labels
require an enclosing switch. These context errors are diagnosed during parsing
and checked again by the semantic analyzer for ASTs built directly by callers.
The manifest now records those early context rejections in `target_parse` while
retaining their rejected validation outcome. Other constraints, including case
values and duplicates, remain independent semantic checks.

Semantic validation requires a scalar `do`/`while` condition and an integer
switch condition. The switch type undergoes integer promotion first. Every case
must have a known integer constant value, and duplicate detection converts that
value to the promoted control type. Thus `unsigned char` controls promote to
`int`, whereas `unsigned int` controls retain unsigned arithmetic; `-1` and
`4294967295U` collide in the latter case. Multiple defaults fail per switch,
while nested switches keep their own case/default sets. Both branches of `if`
statements and all labels are checked even when unreachable at runtime.

Case folding supports the current integer literal/arithmetic, character, cast,
conditional, and known-size `sizeof` facts. Supported constant array-bound
arithmetic is also folded into resolved type extents so that, for example,
`case sizeof(int[1+2]):` can be checked. Parser AST bounds remain unchanged.
Calls, ordinary variable values, assignments, comma expressions, and invalid
constant arithmetic do not supply case constants. Step 6 also supplies prior enum
constants and validated constant array bounds to the same case-folding machinery.

New visitor overloads have throwing defaults, preserving source compatibility
for existing visitors. The legacy `Evaluate` visitor still reports these statements
as unsupported. Step 8's `Interpreter` executes them, and ICode/HTML now emit
numeric do/switch control flow. Tests cover both the new execution and the legacy
visitor's explicit errors.

```bash
./build/cmake/ls-c --syntax-program examples/statements.c
./build/cmake/ls-c --check-program examples/statements.c
```

## Aggregates and initialization

Step 6 adds named and anonymous struct/enum definitions, scoped struct forward
references, complete fixed arrays, and positional initialization. Use
`ls-c --check-program examples/aggregates.c` to validate the combined
example without executing it.

Syntax tags carry an optional `Type::definition`, an `AggregateDefinition`
containing member `DeclarationNode`s or `Enumerator`s. Enumerators retain their
name token and optional value expression. Every member declarator and initializer
has a source location. `InitializerListNode` is an initializer-only expression
container whose ordered `elements` retain explicit nested braces; it cannot be
used as an ordinary expression. Its visitor overload has a throwing default so
existing visitors remain source compatible. Type/declaration printers retain
definitions and initializer nesting, including grouped object/function prototypes.

Validation creates canonical tag identities and a shared `Type::layout` for each
resolved tag. Forward references observe completion of that same layout; parser
syntax types remain unchanged. Layout state is completed by the analyzer, with
`complete`, `size`, `alignment`, and `const_member` available on resolved types.
Members use natural alignment capped at eight bytes, with padding before members
and at the end of structs. Enum objects use signed 32-bit `int` representation;
enumerators have type `int` and must fit its range. `ScalarDataModel::size_of`
works on complete resolved aggregates, and member expression facts include
`member_offset` relative to the immediate struct base. No host compiler ABI is
inferred; these are the subset's fixed LP64 rules.

Tags and ordinary identifiers have separate scopes; member names have their own
namespace and do not hide typedefs. Validation rejects duplicate members/tags,
unknown members, incomplete objects, by-value recursion, flexible arrays, and
function/void members. Pointer self-references are valid. `.` and `->` preserve
member qualifiers and lvalue categories. A struct containing const subobjects
cannot be assigned as a whole; its other writable members remain assignable.
Compatible struct assignment, conditional expressions, value parameters, and
returns are type checked, while aggregate copy initialization remains excluded.

Prior enumerators, integer/character constants, integer casts, `sizeof` complete
types, and supported operators supply integer constant facts for enum values,
array bounds, and cases. Division by zero, signed overflow, invalid shifts,
variable references, calls, and comma/assignment/increment expressions cannot
supply a required constant value. Short-circuit and conditional folding permits
unevaluated invalid arithmetic where C permits it, while still checking types and
names in all branches. Narrow standard, octal, and hexadecimal escapes are
decoded for character constants and concatenated string lengths. Wide/UTF literals
and universal escapes remain excluded.

Explicit array bounds must be positive integer constants and object sizes must
fit the model. Initializers recursively check scalar conversions and positional
array/struct shape, reject excess elements, and allow trailing commas and omitted
elements (implicit zero initialization under C rules). Omitted outer array bounds
are inferred from initializer counts or decoded character strings. A string's
trailing null may be omitted when an explicit character-array extent exactly
matches its contents. One pair of braces is accepted around scalar expressions or
character strings. Scalar initializer expressions retain normal local declaration
visibility; file-scope elements require arithmetic constants or null pointers.
Completed inferred types are available from semantic symbols; AST bounds are not
rewritten.

The `aggregates` CTest suite checks syntax/validation, source locations, layout
and expression facts, parse/print/parse stability, analyzer reuse, malformed input,
and explicit unsupported initializer visitor errors. The fixture runner now checks
all step 1–7 semantic expectations. Flexible-array declarations, brace elision,
and aggregate copy initializers are parsed as syntax and rejected by validation;
their manifest entries retain semantic rejection. Designators, unions, bit-fields,
and enum forward declarations retain explicit parser rejections. The evaluator
and code generators report aggregate definitions/brace initialization as unsupported;
the validated `Interpreter` supplies aggregate storage, zero filling, and execution.

## Publication and verification

Step 7 completes the parser milestone. The README contains the current capability
matrix, and `examples/subset.c` combines tagged/recursive pointer types, enums,
inferred arrays of structs, nested initializers, narrow strings, qualified pointer
parameters, struct-valued calls, member access, loops, switch labels, dangling
`else`, and numeric expressions. It is also executable through `ls-c --run-program examples/subset.c` after
step 8, returning 2.

The final manifest is enforced by `subset-fixtures` for all milestones. Atomic
qualifiers, `inline`, wide literals, and address initializers remain syntax
acceptances with semantic rejection, matching the parser/validator separation.
They have no validated acceptance. Other exclusions with type-dependent shape
checks, such as flexible arrays and brace elision, also fail during validation.
Preprocessor directives, complex types, old-style parameters, compound literals,
generic selection, ordinary labels/`goto`, and static assertions now report their
excluded feature explicitly. Diagnostic wording is not frozen beyond the named
feature and source location.

`subset-publication` reparses every positive fixture and the combined example,
validates both trees, checks stable printed C and AST shape, and audits all 26
concrete AST node kinds, including legacy `AssignmentNode`. The traversal includes
embedded aggregate definitions, enum expressions, parameter declarations, and
array bounds. It checks source locations and nonempty printers, verifies throwing
defaults for visitors implementing only the original interface, and checks the
evaluator/ICode/HTML visitors' explicit errors for unsupported expression,
initializer, and control-flow nodes. Additional assertions check the combined
example's aggregate sizes, function signature, and dangling-`else` association.
The suite rejects 34 truncated inputs and four malformed token sequences with
useful expected-syntax and source diagnostics.

When a system `cc`, `gcc`, or `clang` is found, CMake registers
`subset-c99-comparison`. This runs the validator and compares all positive fixture
originals and printed translation units with the selected compiler using
`-std=c99 -pedantic-errors -fsyntax-only`. Generated files stay in the build tree.
The comparison neither links nor runs C; it verifies acceptance and printed syntax,
not execution equivalence, full C99 conformance, host ABI agreement, or acceptance
of deliberately excluded C features. Positive examples avoid compiler extensions
and undefined behavior. Compiler unavailability is reported during configuration;
select an installed compiler explicitly with `-DMXPARSER_C99_COMPILER=/path/to/cc`.

```bash
cmake -S . -B build/cmake -DBUILD_TESTING=ON
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
ctest --test-dir build/cmake -R 'subset-(fixtures|publication|c99-comparison)' --output-on-failure
```

The validated execution API and its limits are described below. Legacy visitors
and illustrative code generation have narrower support than `Interpreter`.

## Validated execution

`Interpreter::load(program)` validates before allocating objects and building
function execution plans. The AST must stay alive until its replacement or the
interpreter's destruction. A failed load leaves the old program available; a
successful load replaces globals and functions. `call(name, arguments)` defaults
to `main` and returns `ExecutionValue` (`monostate`, signed/unsigned 64-bit integer,
or `double`). The public boundary exchanges scalars; object pointers and struct
values are supported between interpreted C functions. Public object-result calls
report an explicit error; expose a scalar C wrapper when needed.

Applications may map external prototypes to C++ functions with
`Interpreter::register_function(name, callback)`. The callback receives typed
`NativeObject` arguments with scalar conversion, checked pointer/array access,
struct member views, and string reads. Parameters are private C value copies;
pointer writes affect the original objects. Views expire after the callback.
Callbacks return scalars or void and cannot reenter the same interpreter.
A function-pointer overload adapts C++ numbers, strings, and object views.
Registrations persist across program loads. See the
[embedding examples and API details](../README.md#calling-c-functions-from-interpreted-c).

The semantic API now exposes resolved object/parameter symbols through
`symbol(declarator)` and `symbol(parameter)`, and complete canonical struct
members through `members(type)`. This metadata remains valid until the next
validation. The interpreter uses symbol identity rather than runtime name lookup,
so typedefs, shadowing, parameters, globals, and block-scope prototypes retain
their validated bindings.

Integer execution uses each expression's resolved type, promotions, and usual
arithmetic conversions. Unsigned results wrap at their target width; signed
arithmetic/left-shift overflow, division by zero, invalid shifts, and out-of-range
floating-to-integer conversions fail. Signed narrowing uses the fixed model's
two's-complement low bits, and signed right shift is arithmetic. Float operations
and conversions round through binary32; double/long double use binary64. Results
retain unsigned precision in the public value variant. Boolean conversion accepts
numeric values and pointers. Short-circuit and conditional expressions evaluate
only the selected operands, and `sizeof` uses semantic size facts without reading
or executing its operand.

Storage consists of typed scalar cells at validated array/member offsets.
Automatic objects have scope lifetimes; file-scope objects have static storage and
zero initialization. Uninitialized automatic scalar reads fail. Initializers fill
explicit positions in order and zero omitted aggregate subobjects; a scalar
initializer cannot read its own indeterminate value. Narrow strings have static,
read-only storage and decode standard/octal/hex escapes. Struct assignment and
argument/return passing copy cell values, including array members, without aliasing
those subobjects. Object pointers stored inside structs remain pointers to their
original objects.

Object references carry an allocation, offset, and originating subobject/array
bounds. Pointer arithmetic, comparisons, and differences preserve those bounds;
forming a one-past address is allowed, accessing it is rejected. Expired object
access, null dereference, read-only writes, unrelated-array pointer subtraction,
and incompatible representation access fail explicitly. Void/object-pointer
round trips preserve identity. Casts do not widen a member pointer's bounds to a
containing struct. Byte-level aliasing into other object representations,
arbitrary host-address access, external libraries, dynamic
allocation functions, and linking are unsupported. This interpreter does not
prove or diagnose every C undefined behavior (including unsequenced access,
restrict violations, and all effective-type rules).

Function plans retain lexical scope paths at each instruction and label. Jumps
leave expired scopes and enter skipped scopes with uninitialized automatic
objects, so switch labels inside nested loops/conditionals work without flattening
their AST. Switch controls execute once, case constants use promoted conversion,
fallthrough follows source order, `break` leaves the nearest loop/switch, and
`continue` reaches the nearest loop's update/test. Calls and full-expression
aggregate temporaries clean up on return or error. Completed global writes are
retained when a later operation fails.

`ExecutionLimits` sets an instruction budget (default 1,000,000 per call/load),
recursion depth (256), and maximum single object size (1 MiB). Expired object
metadata is retained until program replacement for reliable dangling-pointer
checks. `ExecutionException` carries runtime source locations; validation still
uses `SemanticException`.

ICode/HTML now emit numeric `do`/`while`, switch dispatch, case/default labels,
fallthrough, and separate break/continue targets. They remain illustrative output,
not executable interpreter plans or native code. Object storage/signatures,
aggregate initialization, string/character literals, pointer operations, members,
subscripts, and `sizeof` have explicit unsupported-operation errors in these
visitors. The old `Evaluate` API is unchanged for compatibility and remains
numeric; use `Interpreter` or `--run-program` for LP64/object execution.

CTest adds `execution`, `subset-example-run`, and, with a system compiler,
`execution-c99-comparison`. The latter builds and executes defined-result examples
on an LP64 host and compares printed scalar results with the explicit `--print-result` mode.
Host-specific `long double` storage is excluded from native size comparisons,
while its fixed-model size is unit tested separately. Tests also cover unsigned
precision, arithmetic conversions, lifetimes, aggregate copies, one-time lvalue
evaluation, nested switch/loop targets, runtime failures, resource limits, failed
load recovery, and successful calls after runtime errors. Generated source,
objects, executables, and comparison manifests stay in the build directory.

## Built-in lsprintf

The validated interpreter reserves the function name `lsprintf` for formatted
application-stdout output. It is a language extension to the C99 subset, available
without a header, declaration, or definition. `SemanticAnalyzer` installs its
function symbol automatically (`SemanticSymbol::builtin` is true), checks a
character-format-pointer first argument and scalar/pointer remaining arguments,
and visits every argument for ordinary name/type validation. User variadic
functions remain excluded. Source function redeclarations/definitions named
`lsprintf` are rejected; local ordinary objects can shadow it, and attempts to
call such an object fail normal call validation.

At execution, every argument is evaluated once. Float arguments promote to double;
small integer/enum arguments receive integer promotions. The format supports
C99 narrow `diouxX`, `fFeEgGaA`, `c`, `s`, `p`, `n`, and `%%` conversions, the
`-+ #0` flags, literal or `*` width and precision, and applicable `hh/h/l/ll/j/z/t/L`
length modifiers. Negative star widths left-align; negative star precisions are
omitted. Integer arguments must have the expected promoted width (32-bit for
unmodified/h/hh conversions, 64-bit for l/ll/j/z/t); integer conversions apply the
model's target signedness/narrowing rules. Floating formats require promoted
double, or long double for `L`. Counts/size/ptrdiff/max-width modifiers follow
the subset's fixed LP64 types. Nonstandard extensions, positional arguments, and
wide `%lc`/`%ls` are rejected explicitly.

`%s` and the format string read bounded interpreter character objects, respecting
object lifetimes and initialization. String precision limits scanning and permits
a non-null-terminated array when its requested characters fit. `%c` may emit an
embedded null byte; returns and `%n` count bytes. `%n` requires a writable pointer
to the correct signed integer type and checks count representability. `%p` formats
a deterministic virtual allocation address plus offset (and a host-style null
pointer representation), never a host allocation address. Pointer arithmetic and
identity continue to use the interpreter's object references.

Each conversion is checked before passing a matching host C++ argument to
`snprintf`; raw interpreted variadic data is never passed directly to the host.
Formatted bytes are buffered and written using `std::cout.write`, which targets
the application's stdout without changing its stream formatting flags. Successful
calls return an `int` byte count; output-stream failures return a negative value.
Formatting errors throw `ExecutionException`. Already evaluated argument side
effects and `%n` stores are retained if a later conversion fails; buffered output
is not written until formatting succeeds.

`ExecutionLimits::output_bytes` defaults to 1 MiB per call and bounds format/string
scanning, field sizes, and buffered output. Scanning also consumes the ordinary
instruction budget. `lsprintf` is part of `Interpreter`/`--run-program`; the legacy
numeric `Evaluate` visitor remains unchanged. See `examples/lsprintf.c`.
`lsprintf` CTest checks stdout capture and host-printf formatting agreement, all
conversion families, length modifiers, star fields, zero bytes, counts, `%n`,
pointer identity, errors, short-circuiting, and output-failure recovery. The
`lsprintf-example` test runs the example; this built-in extension is separate from
the ordinary C99 fixture/compiler comparison corpus.

`ls-c --run-program filename` prints only the program's own stdout output; it does
not append the return value from `main`. The optional `--print-result filename`
mode runs the same interpreter and appends `Result: <value>` for debugging and
native-result comparisons. The legacy `Parser::parse()` path also omits its
result summary while retaining its existing intermediate-code and HTML output.

## Interactive execution

`ls-c --repl` (also the no-argument invocation) provides a C-expression REPL.
Unix terminals use GNU Readline; Windows terminals use `ReadConsoleW` with cooked
line editing, Unicode-to-UTF-8 conversion, and no Readline dependency. Redirected
stdin uses line input without prompts on both platforms. Multiline bracketed input
and block comments use a continuation prompt; errors do not terminate the loop.
Readline history is limited to 500 lines in memory. Commands include `:help`,
`:quit`, `:exit`, `:reset`, and `:cancel`; `exit()` and `quit()` also exit.

`Interpreter::submit(source)` accepts interactive declarations/functions or one
expression, returning an optional display string. A final semicolon suppresses an
expression's display; void has no display. Numeric results retain their resolved
signedness and width, pointers show virtual addresses, and structs have summaries.
A function can be defined across lines by opening its brace on the first line.
`--repl filename` loads declarations and initializers without calling `main`.

Submission parsing and a separate verification pass check the accepted source
history plus the proposed input before extending the live semantic environment.
Existing symbol identities, canonical tag layouts, globals, object references,
and function bindings remain live. Only the new input's initializer or expression
executes: history is not replayed. Owned AST fragments keep expression facts valid
until session replacement. An expression runs in a temporary frame so function
calls and aggregate temporaries receive normal lifetime cleanup.

Interactive global initializers can use runtime expressions/calls, unlike normal
file-scope initializers. This relaxation is private to interactive analysis;
`load()` and `--check-program` retain the file subset's constant-initializer rules.
Definitions still follow C duplicate/visibility rules. Compile/type failures do
not commit the input. Runtime failures retain accepted declarations and completed
writes, so failed initializations can leave zero/partially initialized bindings;
`:reset` clears all state. Execution budgets apply to each submission.

CMake uses MSVC warning flags on MSVC and portable overflow checks replace the
GCC-only arithmetic builtins. Readline is linked only to the Unix CLI; it is not a
parser-library dependency. Windows has no Readline discovery/link requirement.
The native C99 runtime comparison is registered only on non-Windows LP64 hosts,
since Windows uses LLP64 while the interpreter deliberately models LP64.

CTest adds persistent-session cases, strict-file-validation regressions, portable
arithmetic boundary cases, and piped CLI transcripts covering multiline input,
preloading, reset/cancel, recovery, and EOF. With Python on Unix, a pseudo-terminal
test also checks real Readline prompts, history recall, line editing, and Ctrl-C/
Ctrl-D. Native Windows console behavior requires a Windows terminal for runtime
verification; piped transcript tests are portable across both backends.
