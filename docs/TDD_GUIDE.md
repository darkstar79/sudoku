# Test-Driven Development (TDD) Guide

**Last Updated:** February 2026

## Overview

This project uses **Test-Driven Development** as a core practice. Writing tests first ensures code correctness, prevents regressions, and drives better API design.

**MANDATORY: All new features and bug fixes must follow the TDD cycle.**

## TDD Workflow (Red-Green-Refactor)

**CRITICAL: Follow this cycle for ALL code changes.**

### 1. 🔴 RED: Write a Failing Test

- Write the test BEFORE implementing the feature
- Test should fail initially (compilation error or assertion failure)
- Defines the expected behavior and API contract

### 2. 🟢 GREEN: Make It Pass

- Write minimal code to make the test pass
- Don't worry about perfect code yet
- Focus on correctness first

### 3. ♻️ REFACTOR: Improve Code Quality

- Clean up implementation (extract helpers, remove duplication)
- Ensure tests still pass after refactoring
- Apply SOLID principles and modern C++ idioms

### Example TDD Cycle

```cpp
// ❌ WRONG: Writing implementation first, then adding tests

// ✅ CORRECT: TDD cycle
// Step 1 (RED): Write failing test first
TEST_CASE("GameValidator validates positions", "[game_validator]") {
    GameValidator validator;
    REQUIRE(validator.isValidPosition({0, 0}));      // Compilation fails: method doesn't exist
    REQUIRE_FALSE(validator.isValidPosition({9, 0})); // Edge case
}

// Step 2 (GREEN): Write minimal implementation to make test pass
// (Implementation details omitted - focus on making the test pass)

// Step 3 (REFACTOR): Improve code quality (extract constants, add [[nodiscard]], etc.)
// while ensuring tests still pass
```

## Test Coverage Requirements

### Target Coverage Metrics

The enforced floors live in `.gcovr.cfg` — that file is the single source of truth for both CI
and `scripts/coverage.sh`. As of 2026-08-03, measured on `main` under local GCC 16.1.1:

- **Line Coverage:** ≥80% (currently 86.9%)
- **Function Coverage:** ≥70% (currently 90.4%)
- **Branch Coverage:** ≥60% (currently 65.9%)

Branch percentages are compiler-specific and are not comparable across toolchains — see
[CODE_QUALITY.md](CODE_QUALITY.md#toolchain-divergence-local-gcc-16-vs-ci-gcc-13) before reading
anything into one.

### What to Test

1. **Happy Path**: Normal, expected usage scenarios
2. **Edge Cases**: Boundary conditions, empty inputs, maximum values
3. **Error Handling**: Invalid inputs, error states, `std::expected` error paths

### What NOT to Test

- Third-party library internals (Qt6, yaml-cpp)
- UI rendering code (QPainter-based widgets - visual testing only)
- Trivial getters/setters without logic

## Test Structure & Naming

### File Organization

```
tests/
├── unit/                          # Unit tests (test single class in isolation)
│   ├── test_game_validator.cpp   # One file per class/component
│   ├── test_puzzle_generator.cpp
│   └── test_save_manager_encryption.cpp
└── integration/                   # Integration tests (test multiple components)
    ├── test_game_view_model_integration.cpp
    └── test_save_manager_integration.cpp
```

### Naming Conventions

- **Test Files:** `test_<component_name>.cpp` (e.g., `test_game_validator.cpp`)
- **Test Cases:** `TEST_CASE("ClassName - Feature Description", "[tag]")`
- **Sections:** `SECTION("Specific behavior or scenario")`
- **Tags:** Use for filtering (`[game_validator]`, `[encryption]`, `[integration]`)

### Example Test Structure

```cpp
TEST_CASE("GameValidator - Move Validation", "[game_validator]") {
    GameValidator validator;
    std::vector<std::vector<int>> board(9, std::vector<int>(9, 0));

    SECTION("Valid moves are accepted") {
        Move move{{0, 0}, 5, MoveType::PlaceNumber, false, std::chrono::steady_clock::now()};
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("Invalid position is rejected") {
        Move move{{9, 0}, 5, MoveType::PlaceNumber, false, std::chrono::steady_clock::now()};
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::InvalidPosition);
    }
}
```

## Testing Best Practices

### 1. Test Happy Path, Edge Cases, and Error Handling

#### Happy Path (Normal Usage)

```cpp
SECTION("Valid puzzle generation") {
    PuzzleGenerator generator;
    auto puzzle = generator.generate(Difficulty::Medium);
    REQUIRE(puzzle.has_value());
    REQUIRE(validator.isValidBoard(puzzle.value()));
}
```

#### Edge Cases (Boundaries, Empty, Extreme)

```cpp
SECTION("Empty board validation") {
    std::vector<std::vector<int>> empty_board(9, std::vector<int>(9, 0));
    REQUIRE_FALSE(validator.isComplete(empty_board));
}

SECTION("Board boundary positions") {
    REQUIRE(validator.isValidPosition({0, 0}));     // Top-left
    REQUIRE(validator.isValidPosition({8, 8}));     // Bottom-right
    REQUIRE_FALSE(validator.isValidPosition({9, 0})); // Out of bounds
}

SECTION("Maximum values") {
    REQUIRE(validator.isValidValue(9));   // Max valid
    REQUIRE_FALSE(validator.isValidValue(10)); // One above max
}
```

#### Error Handling (std::expected, std::optional)

```cpp
SECTION("Load non-existent save returns error") {
    SaveManager manager("./test_saves");
    auto result = manager.loadGame("non_existent_id");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::FileNotFound);
}

SECTION("Invalid encryption key returns error") {
    auto result = encryption_manager.decrypt(encrypted_data, "wrong_password");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == EncryptionError::AuthenticationFailed);
}

SECTION("Empty position search returns nullopt") {
    std::vector<std::vector<int>> full_board = createFullBoard();
    auto pos = validator.findEmptyPosition(full_board);
    REQUIRE_FALSE(pos.has_value());
}
```

### 2. Use Test Fixtures for Common Setup

#### RAII Helper Classes

```cpp
// Use RAII helper classes for automatic resource cleanup
// Example: TempTestDir creates temporary directory in constructor,
// removes it in destructor (guaranteed cleanup even if test fails)

TEST_CASE("SaveManager file operations", "[save_manager]") {
    TempTestDir temp_dir;  // Auto-cleanup on test exit
    SaveManager manager(temp_dir.path());
    // ... tests run with isolated temporary directory ...
    // Directory automatically removed when temp_dir goes out of scope
}
```

#### Helper Functions

```cpp
// Reusable test data builders
SavedGame createTestGame() {
    return SavedGame {
        .current_state = {{5, 3, 0, ...}, ...},
        .original_puzzle = {{5, 3, 0, ...}, ...},
        .notes = createEmptyNotes(),
        .difficulty = Difficulty::Medium,
        .puzzle_seed = 42,
        // ... all fields initialized
    };
}

// Reusable assertion helpers
bool gamesAreEqual(const SavedGame& a, const SavedGame& b) {
    if (a.current_state != b.current_state) return false;
    if (a.difficulty != b.difficulty) return false;
    // ... comprehensive field comparison
    return true;
}
```

### 3. Test Dependency Injection with Mocks

#### Mock Implementations

```cpp
// Create mock for testing in isolation
class MockSaveManager : public ISaveManager {
public:
    [[nodiscard]] std::expected<std::string, SaveError>
    saveGame(const SavedGame& game, const SaveSettings& settings) const override {
        save_called_ = true;
        return "mock_save_id";
    }

    mutable bool save_called_ = false;
};

TEST_CASE("GameViewModel auto-save behavior", "[game_view_model]") {
    auto mock_save_manager = std::make_shared<MockSaveManager>();
    GameViewModel view_model(validator, generator, mock_save_manager, stats);

    view_model.placeNumber({0, 0}, 5);

    REQUIRE(mock_save_manager->save_called_);  // Verify auto-save triggered
}
```

### 4. Test Error Paths with std::expected

**CRITICAL: Test both success AND error paths for all `std::expected` returns.**

```cpp
TEST_CASE("SaveManager error handling", "[save_manager]") {
    TempTestDir temp_dir;
    SaveManager manager(temp_dir.path());

    SECTION("Successful save returns save_id") {
        auto game = createTestGame();
        auto result = manager.saveGame(game, SaveSettings{});
        REQUIRE(result.has_value());
        REQUIRE_FALSE(result.value().empty());
    }

    SECTION("Read-only directory returns PermissionDenied") {
        fs::permissions(temp_dir.path(), fs::perms::owner_read);
        auto result = manager.saveGame(createTestGame(), SaveSettings{});
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::PermissionDenied);
    }

    SECTION("Corrupted file returns DeserializationFailed") {
        // Create corrupted save file
        std::ofstream(temp_dir.path() / "corrupted.sav") << "invalid yaml!!!";
        auto result = manager.loadGame("corrupted");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::DeserializationFailed);
    }
}
```

### 5. Test Observable Pattern Notifications

```cpp
TEST_CASE("Observable notifies observers on state change", "[observable]") {
    Observable<int> observable(42);
    int notification_count = 0;
    int last_value = 0;

    observable.subscribe([&](int new_value) {
        notification_count++;
        last_value = new_value;
    });

    SECTION("set() triggers notification when value changes") {
        observable.set(100);
        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 100);
    }

    SECTION("set() does NOT notify when value is same") {
        observable.set(42);  // Same as initial value
        REQUIRE(notification_count == 0);
    }

    SECTION("update() always checks for changes") {
        observable.update([](int& val) { val = 200; });
        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 200);
    }
}
```

## Running Tests

### Quick Test Execution

```bash
# Run all tests (unit + integration)
./build/Release/bin/tests/unit_tests && ./build/Release/bin/tests/integration_tests

# Run specific test case by name
./build/Release/bin/tests/unit_tests "GameValidator - Move Validation"

# Run tests with specific tag
./build/Release/bin/tests/unit_tests "[game_validator]"

# Verbose output for debugging
./build/Release/bin/tests/unit_tests --success
```

### Test Coverage Analysis

```bash
# Generate coverage report after running tests
./scripts/coverage.sh

# View detailed HTML report
./scripts/coverage.sh html
firefox coverage_html/index.html
```

### Debug Failed Tests

```bash
# Build and run in debug mode
cmake --build --preset conan-debug
gdb --args ./build/Debug/bin/tests/unit_tests "Failing Test Name"
(gdb) run
(gdb) bt full
```

## CI/CD Infrastructure

### Automated Quality Gates (GitHub Actions)

- **Workflow:** `.github/workflows/ci.yml`
- **Triggers:** Every push and pull request
- **Jobs:**
  1. **Code Quality** (~2-3 min): Format check + static analysis
  2. **Build and Test** (~5-8 min): Compile + unit/integration tests
  3. **Coverage Analysis** (~6-10 min): Coverage ≥80% enforcement

**All quality gates must pass** before merging to main:

- ✅ Code formatted (clang-format)
- ✅ No static analysis errors (clang-tidy)
- ✅ All tests pass
- ✅ Coverage ≥80% line and function

### Local Pre-Commit Hooks (Optional)

```bash
./scripts/setup-hooks.sh  # One-time setup
```

Runs **format check only** on staged files (~2 seconds). Clang-tidy and tests run in CI.

To opt-in to local clang-tidy (slow, ~12s/file):

```bash
TIDY=1 git commit -m "message"
```

### Running Checks Locally

```bash
./scripts/format.sh check      # Check formatting (fast)
./scripts/tidy.sh check        # Run static analysis (slow, ~12 min for all files)
./scripts/coverage.sh summary  # Generate coverage report
```

### When Tests Fail

- **NEVER commit failing tests** - Fix them first
- **NEVER disable tests** - Fix the underlying issue
- **NEVER lower coverage thresholds** - Add tests to improve coverage
- **CI blocks merges** automatically if quality gates fail

## Common Testing Pitfalls

### ❌ WRONG: Testing implementation details

```cpp
TEST_CASE("BoardSerializer uses YAML map format") {
    // Testing internal implementation, not behavior
    auto yaml = serializer.serializeInternal(board);
    REQUIRE(yaml.IsMap());
}
```

### ✅ CORRECT: Testing public API behavior

```cpp
TEST_CASE("BoardSerializer round-trip preserves data") {
    auto serialized = serializer.serialize(board);
    auto deserialized = serializer.deserialize(serialized);
    REQUIRE(deserialized == board);
}
```

### ❌ WRONG: Fragile tests with hardcoded values

```cpp
TEST_CASE("PuzzleGenerator creates puzzle") {
    auto puzzle = generator.generate(Difficulty::Easy);
    REQUIRE(puzzle.value()[0][0] == 5);  // Fragile: depends on RNG
}
```

### ✅ CORRECT: Testing invariants, not specific values

```cpp
TEST_CASE("PuzzleGenerator creates valid puzzle") {
    auto puzzle = generator.generate(Difficulty::Easy);
    REQUIRE(puzzle.has_value());
    REQUIRE(validator.isValidBoard(puzzle.value()));
    REQUIRE(hasUniqueSolution(puzzle.value()));
}
```

### ❌ WRONG: Tests with side effects

```cpp
TEST_CASE("SaveManager saves to disk") {
    SaveManager manager("/home/user/.sudoku");  // Modifies real user data!
    manager.saveGame(game, settings);
}
```

### ✅ CORRECT: Tests use isolated temporary directories

```cpp
TEST_CASE("SaveManager saves to disk") {
    TempTestDir temp_dir;  // Isolated, auto-cleanup
    SaveManager manager(temp_dir.path());
    manager.saveGame(game, settings);
}
```

## Test-Driven Development Checklist

Before marking a feature complete:

- ✅ **TDD Cycle**: Tests written BEFORE implementation
- ✅ **Happy Path**: Normal usage scenarios tested
- ✅ **Edge Cases**: Boundaries, empty inputs, maximum values tested
- ✅ **Error Handling**: All `std::expected` error paths tested
- ✅ **std::optional**: `nullopt` cases tested
- ✅ **Observable Pattern**: Notification behavior tested (if applicable)
- ✅ **Dependency Injection**: Mocks used for isolated testing
- ✅ **Test Isolation**: No shared state, no side effects
- ✅ **RAII Cleanup**: Temporary resources cleaned up automatically
- ✅ **Coverage Targets**: ≥80% line/function coverage maintained
- ✅ **All Tests Pass**: Both Debug and Release builds

## Additional Resources

- [Catch2 Documentation](https://github.com/catchorg/Catch2/tree/devel/docs)
- [TEST_SUITE_ANALYSIS_REPORT.md](TEST_SUITE_ANALYSIS_REPORT.md) - Current test coverage and structure
- [CODE_QUALITY.md](CODE_QUALITY.md) - Code formatting and static analysis tools
