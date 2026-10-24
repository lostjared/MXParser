# Basic C subset contract

This is the acceptance contract for README roadmap steps 2–7, not a claim of
implemented C99 support. Step 1 defines this boundary and records the baseline.
The [fixture manifest](../tests/subset/manifest.tsv) maps examples to milestones.

## Capability matrix

"Partial" means the restrictions below matter. `SemanticAnalyzer` now provides
an independent validation pass for the implemented grammar, including code that
never executes. The target validation column still includes upcoming aggregate
and statement features. Execution support remains the numeric interpreter subset.

| Feature | Current parsing | Current validation / execution | Target parsing / validation | Step | Acceptance fixtures |
| --- | --- | --- | --- | --- | --- |
| Arithmetic, assignments, calls, comma, `?:`, casts | Yes | Static operand/lvalue/conversion checks; numeric execution | Implemented for current types | 2, 4 | `expressions`, `invalid-lvalue`, `bad-operands` |
| Integer, floating, character, adjacent string literals | Yes | Numeric only | Yes / typed, decoded literals | 2, 6 | `literals`, `scalars` |
| Scalar types and qualifiers | Structured scalar/qualified types | LP64 validation and qualifier checks; interpreter still uses signed 64-bit storage/double arithmetic | Implemented for current types | 2, 4 | `scalars`, `bad-type`, `const-write`, `bad-restrict` |
| Numeric globals, locals, multiple declarators, block scopes | Yes; independent derived types | Static lexical resolution and redeclaration checks; numeric execution | Implemented | 3, 4 | `numeric-program`, `mixed-declarators`, `duplicate-local` |
| Prototypes, definitions, typed parameters, recursion, void | Yes | Static compatibility, arity, argument and return checks; direct numeric calls execute | Implemented for typed prototypes | 3, 4 | `numeric-program`, `prototype-conflict`, `bad-call`, `bad-return` |
| Scoped typedefs and ordinary-name shadowing | Yes; expression API can also register type names | Numeric aliases execute; parser tracks declaration visibility | Yes / scoped typedef disambiguation implemented | 3 | `typedef-scope`, `bad-typedef` |
| Pointers, arrays, parenthesized and abstract declarators | Yes within the chosen restrictions; array parameters adjust to pointers | No pointer/array execution; bounds await semantic validation | Yes / bound and complete-type checks | 3, 6 | `declarators`, `abstract-types`, `array-parameter`, `bad-declarator` |
| `sizeof`, address/dereference, `.`, `->`, subscripting | Yes | Static pointer/complete-type checks; member access diagnoses incomplete structs until definitions land | Member definitions/layout remain for step 6; no execution | 4, 6 | `expressions`, `abstract-types`, `aggregates`, `unknown-member` |
| Blocks, empty statements, `if`/`else`, `while`, `for`, jumps | Yes | Static scalar-condition and jump-context checks; numeric execution | Implemented for existing statements | 4, 5 | `numeric-program`, `dangling-else`, `bad-break`, `bad-continue` |
| `do`/`while`, `switch`, `case`, `default`, fallthrough | Yes; label order and nesting preserved | Static condition, constant-case, duplicate/default and context checks; execution/ICode deferred | Implemented for existing integer types | 5 | `structured-statements`, `bad-case`, `duplicate-case`, `duplicate-default`, `switch-continue` |
| Struct definitions and forward declarations | References and forward declarations | Separate scoped tags, identity and incomplete-type checks; no layout/member definitions | Definitions/member lookup remain for step 6 | 4, 6 | `aggregates`, `incomplete-object`, `recursive-object` |
| Enum definitions and integer constant expressions | Tag references only | No enum/bound validation | Yes / constant values and positive array bounds | 6 | `aggregates`, `bad-bound`, `nonconstant-bound` |
| Scalar, positional brace, and character-array string initializers | Scalar expressions only | Numeric only | Yes / type, shape, and bounds checking | 6 | `initializers`, `bad-initializer` |
| File-scope `static` and `extern` | Preserved in AST and printing | Static storage/linkage conflict checks and compatible repeated declarations | Implemented within one translation unit; no linking | 3, 4 | `storage`, `bad-storage` |
| AST source locations and useful diagnostics | AST start locations and parser exception locations | Line, column when available, offending token, expected syntax; lexer reason retained | Implemented for current grammar | 2 | `missing-semicolon`, `unexpected-eof` |
| Combined translation unit and AST round trip | Numeric subset | Numeric execution | Yes / all included features together | 7 | `combined` |
| Excluded features listed below | Some fragments accepted | No support contract | Explicit unsupported-feature diagnostic | 1, 7 | All `excluded-*` fixtures |

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
  integer constant expressions. Allow omitted bounds only when an initializer
  determines the size, or in an array parameter adjusted to a pointer.
- Functions have typed prototypes and non-variadic parameter lists; definitions
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
  returns are included in semantic validation, with execution deferred.
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
`parse_program()` without evaluation or output files. Successfully parsed inputs
owned by steps 1–5 also check the manifest's semantic expectation through
`SemanticAnalyzer`. A semantic rejection can therefore be a syntax acceptance.
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
pointer to an immutable `mx::Type`. Inspect `kind` before reading the corresponding
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
or functions are rejected. Aggregate definitions and brace initializers remain
for step 6. Numeric aliases execute in the existing interpreter; typedef
declarations create no runtime objects. Pointer and array execution, and block
function-prototype execution, remain unsupported. No C integer-width, linkage,
or full compatibility semantics are implied by this parser milestone.

Try the syntax example with:

```bash
./build/cmake/parse-expr --syntax-program examples/declarations.c
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
./build/cmake/parse-expr --check-program examples/functions.c
./build/cmake/parse-expr --check-program examples/declarations.c
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
and member access on currently incomplete structs fail explicitly.

`ScalarDataModel` defines the LP64 sizes specified above, integer promotions,
usual arithmetic conversions, signedness, and floating ranks. It can report sizes
of scalars, pointers, and arrays with known literal bounds. The validator types
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

The interpreter has not adopted this LP64 arithmetic model: it still uses signed
64-bit integer storage and double arithmetic as documented above. Static type
approval therefore does not promise C-equivalent evaluation or pointer execution.
Bounds' positivity and full integer-constant evaluation, excluded variable-length
arrays, enum constants, aggregate definitions/layout/member lookup, and brace
initializers remain for step 6. Member checks currently diagnose invalid or
incomplete bases rather than resolving fields. `sizeof` validates unevaluated
operand types; its numeric fact is available only when the current model knows
the size. This pass does not prove initialization, all-path returns, reachability,
runtime bounds, alias safety, or termination. Steps 5–6 extend it alongside their
new AST nodes and grammar. Step 5's structured statements are described below.

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
constant arithmetic do not supply case constants. Enum constants and the full
bound/constant-expression work remain for step 6.

New visitor overloads have throwing defaults, preserving source compatibility
for existing visitors. The evaluator, intermediate-code generator, and HTML
visitor currently report these statements as unsupported. This is deliberate:
their execution is planned for step 8. The statement suite verifies those errors
and evaluator recovery, rather than claiming runtime fallthrough support.

```bash
./build/cmake/parse-expr --syntax-program examples/statements.c
./build/cmake/parse-expr --check-program examples/statements.c
```
