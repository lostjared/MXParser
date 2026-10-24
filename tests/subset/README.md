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
| `target_parse` | Expected syntax result after steps 2–7, including explicit rejection of excluded features |
| `target_validation` | Expected result after both target parsing and semantic validation |
| `target_diagnostic` | Required diagnostic meaning for target rejection, or `-` for acceptance |

Diagnostic meanings do not prescribe exact wording or claim that today's parser
already produces them. Semantic errors such as an invalid lvalue or undeclared
name can have `current_parse=accept` and `target_validation=reject`. Some excluded
features also parse today and must be rejected by the future validated subset.
Parsing acceptance records syntax only. Step 3 also verifies independent types
for `mixed-declarators.c` and scoped typedef behavior in the `declarators` suite.
Array-bound fixtures may parse before their constraints can be checked; constant
expression and positive-bound validation remain work for step 6.

Run all baseline checks from the repository root:

```bash
cmake -S . -B build/cmake -DBUILD_TESTING=ON
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

Run only fixture checks with `ctest --test-dir build/cmake -R subset-fixtures
--output-on-failure`. This checks current syntax expectations and parser
diagnostics with source locations and token columns when available. For inputs
owned by steps 1–5 that parse successfully, it also checks `target_validation`
using the independent semantic analyzer. No source code executes. Later acceptance expectations are
data for upcoming milestones, not expected-failure tests. When implementing a
feature, update its baseline and add AST or semantic assertions to verify the
target behavior. Preserve negative cases and the existing expression/function
regressions.

Step 5 updates `target_parse` for invalid loop/switch jump and label contexts to
match the parser's early context diagnostics. Their `target_validation` remains
`reject`; the statement suite also checks semantic contexts using constructed
ASTs. Duplicate cases/defaults remain syntax acceptances with semantic rejections.
