# Code Quality Tools and Guidelines

This document describes the code quality tools and processes for the sudoku-cpp project.

## Overview

The project uses six main code quality tools:

1. **clang-format** - Automated code formatting
2. **clang-tidy** - Static analysis and linting
3. **cppcheck** - Complementary static analysis (buffer overflows, null derefs, memory leaks)
4. **PMD CPD** - Copy-paste detection
5. **gcovr** - Test coverage analysis
6. **AddressSanitizer + UBSan** - Runtime memory error and undefined behavior detection

All tools automatically **exclude third-party code** and build artifacts.

## Code Formatting (clang-format)

### Quick Start

```bash
# Check which files need formatting
./scripts/format.sh check

# Format all project files
./scripts/format.sh

# Format specific files
./scripts/format.sh src/core/game_state.cpp
```

### CMake Integration

```bash
# Format all files
cmake --build --preset conan-release --target format

# Check formatting (dry-run)
cmake --build --preset conan-release --target format-check
```

### Formatting Rules

Configuration file: `.clang-format`

**Key Features:**
- Based on LLVM style with C++23 customizations
- **InsertBraces: true** - Automatically adds braces around all if/while/for statements
- 120 character line limit
- 4-space indentation (no tabs)
- Left-aligned pointers and references (`int* ptr`)
- Automatic include sorting and grouping

**Bracing Policy (ENFORCED):**
```cpp
// ❌ BAD - Single-line if without braces
if (condition) return;

// ✅ GOOD - Always use braces
if (condition) {
    return;
}
```

**Third-Party Code Exclusion:**
- Build directory and Conan cache are excluded
- Only project source code in `src/core/`, `src/model/`, `src/view/`, `src/view_model/`, and `tests/` is formatted

### Requirements

- clang-format version 15+ (for InsertBraces feature)
- Current system: clang-format 21.1.7 ✅

## Static Analysis (clang-tidy)

### Quick Start

```bash
# Run analysis
./scripts/tidy.sh check

# Run with verbose output
./scripts/tidy.sh check --verbose

# Apply automatic fixes
./scripts/tidy.sh fix

# Generate report
./scripts/tidy.sh report
```

> **Performance note:** `tidy.sh` takes 7–15 minutes per run (full recompilation through
> clang-tidy). When investigating warnings, run it **once** and save the output, then
> filter the saved file for all follow-up queries:
>
> ```bash
> ./scripts/tidy.sh check 2>&1 > /tmp/tidy_output.txt   # run once
> grep -c "warning:" /tmp/tidy_output.txt                # count
> grep "warning:" /tmp/tidy_output.txt | sed 's/.*\[\(.*\)\]/\1/' | sort | uniq -c | sort -rn  # by category
> grep "warning:.*some-check" /tmp/tidy_output.txt       # locations for a specific check
> ```
>
> Re-run the script (invalidating the cached output) only when:
>
> - Source files have been modified to fix warnings
> - `.clang-tidy` configuration has changed (checks added/removed/suppressed)
> - New source files have been added to the project

### Incremental Check (Changed Lines Only)

`scripts/tidy-diff.sh` runs clang-tidy only on the lines you changed, using
`clang-tidy-diff.py`. It ignores pre-existing warnings in untouched code and
runs in parallel — typically **5–30 seconds** for a normal commit.

```bash
# Check staged changes before committing (same as TIDY=1 git commit)
./scripts/tidy-diff.sh

# Check all changes on a feature branch vs main (for PR review)
./scripts/tidy-diff.sh main

# Check the last N commits
./scripts/tidy-diff.sh HEAD~3
```

**Integrate into commit workflow:**

```bash
TIDY=1 git commit -m "my message"   # runs tidy-diff automatically
```

**Reliability caveat:** changes to `.cpp` files are fully covered. Changes to
`.h` files are only covered if the header is a translation unit in
`compile_commands.json` (uncommon). For header-only changes, follow up with
`./scripts/tidy.sh check` or use the cached output approach above.

### CMake Integration

```bash
# Run clang-tidy on all files
cmake --build --preset conan-release --target tidy

# Apply automatic fixes
cmake --build --preset conan-release --target tidy-fix
```

**Automatic Analysis:**
- clang-tidy runs automatically during compilation
- Configuration prevents warnings from system headers

### Enabled Checks

Configuration file: `.clang-tidy`

**Categories:**
- `bugprone-*` - Bug-prone code patterns
- `cert-*` - CERT secure coding guidelines
- `clang-analyzer-*` - Deep static analysis
- `cppcoreguidelines-*` - C++ Core Guidelines
- `modernize-*` - Modern C++ features (C++23)
- `performance-*` - Performance optimizations
- `readability-*` - Code readability

**Key Settings:**
- Function complexity threshold: 25
- Function line threshold: 100
- Minimum variable name length: 1 (allows single-char loop/math variables)
- Requires `[[nodiscard]]` on appropriate functions
- Enforces `std::uint8_t` for small enums
- Requires braces around single-statement conditionals

**Excluded Checks:**
- Magic numbers (context-dependent)
- Non-private member variables in classes (data structures)
- Trailing return types (preference)
- Easily swappable parameters (unavoidable in some APIs)

### Common Warnings and Fixes

**Performance Issues:**
```cpp
// Warning: performance-enum-size
enum class MoveType {  // Uses 4 bytes
    Place, Remove, Note
};

// Fix: Specify smaller base type
enum class MoveType : std::uint8_t {  // Uses 1 byte
    Place, Remove, Note
};
```

**Modernization:**
```cpp
// Warning: modernize-use-nodiscard
virtual bool isComplete() const = 0;

// Fix: Add [[nodiscard]] attribute
[[nodiscard]] virtual bool isComplete() const = 0;
```

**Guidelines Compliance:**
```cpp
// Warning: cppcoreguidelines-special-member-functions
class IGameValidator {
public:
    virtual ~IGameValidator() = default;
    // Missing: copy/move constructors and assignment operators
};

// Fix: Explicitly delete or define all special members
class IGameValidator {
public:
    virtual ~IGameValidator() = default;
    IGameValidator() = default;
    IGameValidator(const IGameValidator&) = delete;
    IGameValidator& operator=(const IGameValidator&) = delete;
    IGameValidator(IGameValidator&&) = delete;
    IGameValidator& operator=(IGameValidator&&) = delete;
};
```

### Customization

**Adding New Checks:**

Edit `.clang-tidy`:
```yaml
Checks: '
  -*,
  bugprone-*,
  your-new-check-*
'
```

**Check-Specific Options:**
```yaml
CheckOptions:
  - key: readability-identifier-length.MinimumVariableNameLength
    value: 3
  - key: performance-for-range-copy.WarnOnAllAutoCopies
    value: false
```

### Editor Integration

**VS Code:**

Install the clangd extension for real-time analysis:
```json
{
    "clangd.arguments": ["--clang-tidy"]
}
```

**CLion:**

Enable clang-tidy in Settings → Editor → Inspections → C/C++ → Clang-Tidy.

### Troubleshooting

**Compilation Database Issues:**

If you see "Could not auto-detect compilation database":
```bash
# Regenerate compilation database
cmake --preset conan-release
```

**Module Compatibility:**

For C++23 projects with modules, ensure:
```cmake
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)  # Disable for clang-tidy compatibility
```

**Performance:**

Large codebases may be slow to analyze:
```bash
# Analyze specific files only
clang-tidy -p=build/Release src/specific/file.cpp

# Use parallel processing
make -j$(nproc) tidy
```

### Third-Party Code Exclusion

**.clang-tidy configuration:**
```yaml
# Only analyze project headers, not third-party
HeaderFilterRegex: 'src/.*\.h$'
```

## Cppcheck Static Analysis

[cppcheck](https://cppcheck.sourceforge.io/) is a static analysis tool that complements clang-tidy by detecting different bug classes: buffer overflows, null pointer dereferences, memory leaks, and CERT/MISRA violations.

### Quick Start

```bash
# Run cppcheck analysis
./scripts/cppcheck.sh

# Generate XML report for CI
./scripts/cppcheck.sh report

# Verbose output with informational messages
./scripts/cppcheck.sh check --verbose
```

### Configuration

The script uses `compile_commands.json` from the build directory for accurate analysis. Key flags:

- `--enable=all` — Enable all check categories
- `--std=c++23` — C++23 standard compliance
- `--error-exitcode=1` — Fail on findings (for CI)
- `--file-filter=*/src/*` — Scan only production code

**Suppressions:**
- `unmatchedSuppression` — Avoid noise from version differences
- `missingIncludeSystem` — System headers are not cppcheck's concern
- `unusedFunction` — Unreliable with partial compilation databases

### Inline Suppressions

To suppress a false positive in code:

```cpp
// cppcheck-suppress [checkId]
someCode();
```

### CI Integration

Runs in the **nightly** workflow as a parallel job. Requires a full build for `compile_commands.json`.

## Copy-Paste Detection (PMD CPD)

[PMD CPD](https://pmd.github.io/pmd/pmd_userdocs_cpd.html) (Copy/Paste Detector) finds duplicated code blocks using the Rabin-Karp algorithm. It catches structural clones even when identifiers differ.

### Quick Start

```bash
# Install PMD (first time only)
./scripts/cpd.sh install

# Run copy-paste detection
./scripts/cpd.sh

# Lower the duplicate threshold
./scripts/cpd.sh check --tokens 75
```

### Configuration

- **Minimum tokens:** 100 (catches meaningful duplicates without excessive noise)
- **Language:** C++
- **Scan scope:** `src/` only (test duplication is expected with the AAA pattern)
- **Requires:** Java runtime

### CI Integration

Runs in the **nightly** workflow as a parallel job. No build step needed — CPD is pure text analysis.

## Include Hygiene (IWYU)

[include-what-you-use](https://include-what-you-use.org/) verifies that each source file directly
includes only the headers it uses — no more, no less. It catches:

- **Redundant includes** — headers included but not directly used
- **Missing direct includes** — relying on a header transitively included by another header
- **Unnecessary transitive dependencies** — fragile includes that break if an indirect chain changes

### Installation and Usage

```bash
# Install (once)
sudo apt-get install iwyu

# Report include issues
./scripts/iwyu.sh

# Apply fixes automatically
./scripts/iwyu.sh --fix
```

Output is saved to `build/iwyu-report/iwyu.log`. A compiled build must exist first
(`cmake --build build/Release`).

### Notes

- Analysis runs on `src/` files only — Qt and third-party headers are excluded
- `-Xiwyu --no_fwd_decls` suppresses forward-declaration suggestions (too noisy in practice)
- After `--fix`, review changes with `git diff` before committing — IWYU suggestions
  occasionally need manual adjustment for Qt headers

---

## Test Coverage (gcovr)

### Quick Start

```bash
# Generate coverage report
./scripts/coverage.sh

# Generate HTML report
./scripts/coverage.sh html

# Generate all formats
./scripts/coverage.sh all

# Clean rebuild with coverage
./scripts/coverage.sh clean
```

### Current Metrics

- **Line Coverage:** 91.2% (target: 80%)
- **Function Coverage:** 90.5% (target: 70%)
- **Branch Coverage:** 59.8% (target: 55%)

### Coverage Reports

| Target | Description | Output |
|--------|-------------|---------|
| `coverage` | Text summary to console | Terminal output |
| `coverage-html` | Detailed HTML report | `coverage_report.html` |
| `coverage-xml` | XML format for CI/CD | `coverage.xml` |
| `coverage-json` | JSON for programmatic use | `coverage.json` |
| `coverage-all` | All formats combined | Multiple files |
| `coverage-lcov` | Legacy lcov format | `coverage_html_lcov/` |

### Interpreting Results

**Coverage Metrics:**
- **Line Coverage:** Percentage of executable lines that were executed
- **Branch Coverage:** Percentage of conditional branches that were taken
- **Function Coverage:** Percentage of functions that were called

**HTML Report Features:**
- **File-by-file breakdown** with line-by-line coverage
- **Color coding:** Red (uncovered), Yellow (partial), Green (covered)
- **Interactive navigation** through source files
- **Summary statistics** and coverage trends

**Quality Thresholds:**

| Coverage Level | Line % | Branch % | Quality |
|----------------|--------|----------|---------|
| Excellent | 90%+ | 85%+ | 🟢 High quality |
| Good | 80-89% | 70-84% | 🟡 Acceptable |
| Poor | <80% | <70% | 🔴 Needs improvement |

### Configuration

Coverage analysis is configured through `.gcovr.cfg`:

```ini
# Minimum thresholds
fail-under-line = 80      # 80% line coverage required
fail-under-branch = 70    # 70% branch coverage required

# HTML report thresholds
html-medium-threshold = 75
html-high-threshold = 90

# Exclusions
exclude = tests/.*, build/.*, /usr/.*
```

### Coverage Exclusions

The following are automatically excluded from coverage analysis:
- Test files (`tests/.*`)
- Build artifacts (`build/.*`)
- Third-party libraries (Conan packages)
- System headers (`/usr/.*`)

### CI/CD Integration

For continuous integration, use the XML output:

```bash
# Generate coverage for CI
./scripts/coverage.sh xml

# Check if coverage meets thresholds
gcovr --config .gcovr.cfg --fail-under-line 80 --fail-under-branch 70
```

### Troubleshooting Coverage

**Coverage Data Not Generated:**
1. Ensure debug build: `CMAKE_BUILD_TYPE=Debug`
2. Run tests before generating reports
3. Check that `--coverage` flags are applied

**Low Coverage Numbers:**
1. Verify tests are comprehensive
2. Check for excluded files in `.gcovr.cfg`
3. Review untested code paths in HTML report

**Performance Issues:**

Coverage builds are slower due to instrumentation:
- Use for testing and CI only
- Don't use coverage flags in release builds
- Clean build directory between coverage runs

### Best Practices for Coverage

1. **Run coverage regularly** during development
2. **Aim for high coverage** but focus on meaningful tests
3. **Review uncovered code** to identify missing test cases
4. **Use branch coverage** to ensure all code paths are tested
5. **Integrate with CI/CD** to prevent coverage regressions

## Workflow Integration

### Pre-Commit Checklist

1. **Format your code:**
   ```bash
   ./scripts/format.sh
   ```

2. **Check for warnings:**
   ```bash
   ./scripts/tidy.sh check
   ```

3. **Run tests:**
   ```bash
   ./build/Release/bin/tests/unit_tests
   ./build/Release/bin/tests/integration_tests
   ```

4. **Check coverage (optional):**
   ```bash
   ./scripts/coverage.sh
   ```

### Automated Checks (GitHub Actions CI/CD)

**Status:** ✅ **ACTIVE** - CI/CD pipeline fully operational

**Workflow:** `.github/workflows/ci.yml`

All pushes and pull requests automatically enforce:
- ✅ Code formatting check passes (clang-format)
- ✅ Zero clang-tidy errors
- ✅ All tests passing (901 test cases, 14,512 assertions)
- ✅ Coverage ≥80% (line and function)

**Quality Gates:**
- **Code Quality Job** (~2-3 min): Formatting + static analysis
- **Build and Test Job** (~5-8 min): Compilation + test execution
- **Coverage Analysis Job** (~6-10 min): Coverage verification + artifact upload

**Local Pre-Commit Hooks:**
```bash
./scripts/setup-hooks.sh  # Install local git hooks for instant feedback
```

**Viewing CI Results:**
- Check the "Actions" tab on GitHub
- Pull request checks show pass/fail status
- Download coverage reports from workflow artifacts

## File Organization

### Configuration Files

- `.clang-format` - Code formatting rules
- `.clang-tidy` - Static analysis configuration
- `.gitignore` - Excludes generated files and third-party code

### Quality Scripts

- `scripts/format.sh` - Code formatting script
- `scripts/tidy.sh` - Static analysis script
- `scripts/coverage.sh` - Test coverage script

### Build Integration

- `CMakeLists.txt` - Contains targets: `format`, `format-check`, `tidy`, `tidy-fix`
- Automatic exclusion of third-party code from all tools

## Common Issues and Solutions

### Issue: "clang-format not found"

**Solution:**
```bash
sudo dnf install clang-tools-extra
```

### Issue: "InsertBraces not working"

**Solution:**
Check version (need 15+):
```bash
clang-format --version
```

### Issue: "Format script is slow"

**Solution:**
Format specific files instead of entire project:
```bash
./scripts/format.sh src/core/game_state.cpp src/model/game_state.cpp
```

## Best Practices

1. **Always format before commit** - Use `./scripts/format.sh`
2. **Fix clang-tidy warnings** - Don't ignore them, they catch real bugs
3. **Write tests for new code** - Maintain >70% coverage
4. **Use `[[nodiscard]]`** - For functions returning important values
5. **Use braces** - Even for single-line conditionals (enforced by formatter)
6. **Keep functions small** - Under 100 lines (clang-tidy warning at 100)
7. **Use modern C++23** - std::expected, designated initializers, ranges

## Version Requirements

- **clang-format:** 15+ (current: 21.1.7) ✅
- **clang-tidy:** Any recent version (current: 21.1.7) ✅
- **gcovr:** 5.0+ (current: installed) ✅
- **CMake:** 3.28+ ✅
- **GCC:** 15+ with C++23 support ✅

## Resources

- [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [clang-format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [clang-tidy Checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html)

## Future Improvements

### Ubuntu 26.04 LTS Migration (April 2026)

When GitHub Actions adds `ubuntu-26.04` runners (Ubuntu 26.04 LTS "Resolute Raccoon",
release April 23, 2026), the following CI/tooling simplifications become possible:

**Ubuntu 26.04 default toolchain:**
- GCC 15 (default)
- LLVM 21 / clang-tidy-21 (full C++23 support out of the box)

**Changes to make after migrating `runs-on: ubuntu-26.04`:**

1. **`nightly.yml`** — Remove the `Install clang-tidy` step that installs
   `clang-tidy-19` and symlinks it. `clang-tidy` (v21) will be available
   via `sudo apt-get install -y clang-tidy` directly.

2. **`scripts/tidy.sh`** — Remove the LLVM version detection block in both
   `get_source_files()` and `run_clang_tidy_check()`. The `llvm_major < 19`
   workarounds (GCC include path injection, header file exclusion) are no
   longer needed. Simplify both functions to always include `.h` files and
   skip the `--extra-arg=-std=c++23` / `-isystem` injection.

3. **`.clang-tidy` / `conanfile.py`** — No changes needed; C++23 settings
   remain correct.

---

## AddressSanitizer + UBSan

The nightly CI pipeline runs the full test suite under ASan (AddressSanitizer) and UBSan (Undefined Behavior Sanitizer) to detect memory errors at runtime: buffer overflows, use-after-free, stack-use-after-return, uninitialized reads, and undefined behavior.

### Running Locally

```bash
# Build with sanitizers
conan install . --build=missing -s build_type=Release -s compiler.cppstd=gnu23
cmake --preset release -DENABLE_SANITIZERS=ON
cmake --build --preset release

# Run with sanitizer options
export ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:halt_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

./build/Release/bin/tests/unit_tests "~[slow]~[pathological]"
./build/Release/bin/tests/integration_tests
```

The `~[slow]~[pathological]` filter excludes tests that are too expensive under sanitizer instrumentation (the unsolvable-board solver test takes 10+ minutes normally).

### CI Configuration

The nightly `sanitizer-check` job (`.github/workflows/nightly.yml`) runs:

- Unit tests with `~[slow]~[pathological]` filter
- All integration tests
- Timeout: 45 minutes

### Known Third-Party Leak Reports

ASan's leak detector reports ~444 KB in ~7,000 allocations at exit. **All of these are in third-party libraries, not in project code.** They are one-time "allocate and never free" patterns -- global caches and singletons that the OS reclaims on exit. They do not grow over time and are not real leaks.

| Library                        | Source                                                                 | Description                                                                      |
| ------------------------------ | ---------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| **libfontconfig**              | `FcConfigValues`, `FcConfigEvaluate`, `FcPatternObjectAddWithBinding`  | Font configuration cache -- global singleton, allocated once during font matching |
| **libxkbcommon**               | `xkb_context_new`, `xkb_keymap_new_from_buffer`, `CompileSymbols`     | Keyboard layout compilation -- one-time allocation during Wayland keyboard init   |
| **libpango / libpangocairo**   | `pango_itemize_with_font`, `pango_layout_check_lines`                  | Font rendering layout caches                                                     |
| **libgtk-3**                   | `gtk_entry_init`, `find_invisible_char`                                | GTK widget init via Qt's `libqgtk3.so` platform theme plugin                     |
| **Qt6WaylandClient**           | `QWaylandDisplay::setupConnection`, `QWaylandInputDevice::Keyboard`    | Wayland display/input device setup -- one-time platform initialization            |
| **Qt6Gui / Qt6Widgets**        | `QGuiApplicationPrivate::init`, `QApplicationPrivate::init`            | Qt application bootstrap -- never freed by design                                |

These leaks appear regardless of which features are used and are present even with a minimal Qt application on Wayland + GTK theme. If ASan leak reports grow significantly beyond this baseline or show stacks through project code (`src/`), that indicates a real leak to investigate.
