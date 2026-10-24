# Subset acceptance fixtures

See the [language contract](../../docs/c-subset.md) for the grammar, support
matrix, exclusions, and diagnostic requirements. Each `.c` file is a complete
parser input, including deliberately malformed files.

`manifest.tsv` is tab-separated, with one row per fixture:

| Column | Meaning |
| --- | --- |
| `file` | Source path relative to the manifest |
| `step` | Roadmap milestone owning the acceptance case |
| `current_parse` | `accept` or `reject` through today's `parse_program()` |
| `target_parse` | Final syntax contract; must equal `current_parse` |
| `target_validation` | Expected result after both target parsing and semantic validation |
| `target_diagnostic` | Required diagnostic meaning for target rejection, or `-` for acceptance |

Diagnostic meanings do not prescribe exact wording. Excluded features additionally
assert keywords naming the unsupported feature. Semantic errors such as an invalid
lvalue or undeclared name can have `current_parse=accept` and
`target_validation=reject`. Some excluded forms parse but must fail validation.
Parsing acceptance records syntax only. Step 3 also verifies independent types
for `mixed-declarators.c` and scoped typedef behavior in the `declarators` suite.
Array-bound fixtures may parse before their constraints can be checked; constant
expression and positive-bound validation are checked from step 6 onward.

Run all baseline checks from the repository root:

```bash
cmake -S . -B build/cmake -DBUILD_TESTING=ON
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

Run only fixture checks with `ctest --test-dir build/cmake -R subset-fixtures
--output-on-failure`. This checks current syntax expectations and parser
diagnostics with source locations and token columns when available. For inputs
from all seven milestones that parse successfully, it also checks
`target_validation` using the independent semantic analyzer. No source code
executes. A mismatch between current and target parsing fails the test. Preserve
negative cases and the existing expression/function regressions when extending
the subset.

Step 5 updates `target_parse` for invalid loop/switch jump and label contexts to
match the parser's early context diagnostics. Their `target_validation` remains
`reject`; the statement suite also checks semantic contexts using constructed
ASTs. Duplicate cases/defaults remain syntax acceptances with semantic rejections.

Step 6 adds aggregate, enum, bound, and initializer validation. The `aggregates`
suite also checks layout/member facts and parse/print stability. Flexible-array
members, brace elision, and aggregate copy initializers now have
`target_parse=accept` and `target_validation=reject`: these exclusions require
semantic type/shape information. Their validated acceptance has not changed.
`combined.c` is also included in the publication and compiler checks.

Step 7 enforces every row and adds named exclusion assertions, ordinary-label and
static-assertion rejection fixtures. Atomic qualifiers, `inline`, wide literals,
and address initializers have `target_parse=accept` and
`target_validation=reject`; validation is their rejection phase.

`subset-publication` checks positive fixture AST/source traversal, stable printing
and reparsing, malformed/EOF input, and explicit unsupported visitor operations.
When a C compiler is available, `subset-c99-comparison` checks every positive
original and printed fixture and `examples/subset.c` with
`-std=c99 -pedantic-errors -fsyntax-only`. It leaves generated C in the build tree
and does not link or execute it. Set `MXPARSER_C99_COMPILER` to select the compiler;
CMake reports when no compiler is available to register this optional test.
