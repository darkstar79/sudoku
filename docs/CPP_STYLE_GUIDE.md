# C++23 Style Guide

This document defines the C++23 coding standards for the sudoku project. These standards ensure safe, maintainable, and modern C++ code.

---

## Table of Contents

- [Modern C++23 Features](#modern-c23-features)
- [[[nodiscard]] Mandate](#nodiscard-mandate)
- [Const Correctness](#const-correctness)
- [Type Safety](#type-safety)
- [DRY Principle (Don't Repeat Yourself)](#dry-principle-dont-repeat-yourself)
- [Memory Safety](#memory-safety)
- [Code Smells Checklist](#code-smells-checklist)
- [AAA Style Reference](#aaa-style-reference)

---

## Modern C++23 Features

### 1. Prefer `std::optional` Over Sentinel Values

**Use `std::optional<T>` when a value may or may not exist.**

```cpp
// ✅ GOOD: Type-safe, self-documenting
std::optional<Position> findEmptyPosition(const Board& board) const {
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0) {
                return Position{.row = row, .col = col};
            }
        }
    }
    return std::nullopt;  // No empty position found
}

// Usage
if (auto pos = findEmptyPosition(board); pos.has_value()) {
    const auto& [row, col] = pos.value();
    // Use row, col
}
```

```cpp
// ❌ BAD: Magic sentinel values
std::pair<int, int> findEmptyPosition(const Board& board) const {
    // Returns {-1, -1} if not found
    for (int row = 0; row < 9; ++row) {
        for (int col = 0; col < 9; ++col) {
            if (board[row][col] == 0) {
                return {row, col};
            }
        }
    }
    return {-1, -1};  // What does this mean?
}

// Usage - error-prone
auto [row, col] = findEmptyPosition(board);
if (row == -1) {  // Easy to forget check
    // Handle not found
}
```

**Why `std::optional` is better:**
- **Type-safe:** Can't accidentally use invalid value without checking
- **Self-documenting:** Return type explicitly states "may not exist"
- **Composable:** Works with `and_then()`, `or_else()`, `value_or()`

---

### 2. Use `std::expected` for Error Handling

**Use `std::expected<T, E>` when an operation can fail with an error.**

```cpp
// ✅ GOOD: Explicit error types, composable
std::expected<SavedGame, SaveError> loadGame(const std::string& save_id) const {
    if (!fileExists(save_id)) {
        return std::unexpected(SaveError::FileNotFound);
    }

    auto decrypted = decryptFile(save_id);
    if (!decrypted) {
        return std::unexpected(SaveError::DecryptionFailed);
    }

    return parseSavedGame(*decrypted);
}

// Usage
auto result = saveManager->loadGame("save_001");
if (result.has_value()) {
    const auto& game = result.value();
    // Use game
} else {
    switch (result.error()) {
        case SaveError::FileNotFound:
            showError("Save file not found");
            break;
        case SaveError::DecryptionFailed:
            showError("Failed to decrypt save file");
            break;
    }
}
```

```cpp
// ❌ BAD: Exceptions for expected errors
SavedGame loadGame(const std::string& save_id) const {
    if (!fileExists(save_id)) {
        throw FileNotFoundException();  // Exception for expected case?
    }
    // ...
}

// Usage - exceptions for control flow
try {
    auto game = saveManager->loadGame("save_001");
} catch (const FileNotFoundException&) {
    // Expected case treated as exception
}
```

**Why `std::expected` is better:**
- **Explicit errors:** Caller knows function can fail
- **No exceptions:** Faster, no stack unwinding
- **Composable:** Chain operations with `and_then()`, `or_else()`
- **Type-safe:** Compiler enforces error handling

---

### 3. Designated Initializers

**Use designated initializers for struct initialization.**

```cpp
// ✅ GOOD: Clear, self-documenting
Position pos{.row = 5, .col = 3};
GameConfig config{
    .difficulty = Difficulty::Hard,
    .hintsEnabled = true,
    .timerEnabled = false
};
```

```cpp
// ❌ BAD: Positional arguments (error-prone)
Position pos{5, 3};  // Which is row, which is col?
GameConfig config{Difficulty::Hard, true, false};  // What do bools mean?
```

---

### 4. Defaulted Comparisons

**Use `= default` for comparison operators.**

```cpp
struct Position {
    size_t row;
    size_t col;

    // ✅ GOOD: Compiler-generated comparison
    bool operator==(const Position&) const = default;
};
```

```cpp
// ❌ BAD: Manual comparison (boilerplate)
bool operator==(const Position& other) const {
    return row == other.row && col == other.col;
}
```

---

### 5. Structured Bindings

**Use structured bindings for decomposing pairs/tuples/structs.**

```cpp
// ✅ GOOD: Clear, concise
auto [row, col] = pos;
for (const auto& [key, value] : map) {
    // Use key, value
}
```

```cpp
// ❌ BAD: Manual decomposition
size_t row = pos.row;
size_t col = pos.col;
for (const auto& entry : map) {
    const auto& key = entry.first;
    const auto& value = entry.second;
}
```

---

### 6. `inline constexpr` for Compile-Time Constants

**Use `inline constexpr` for compile-time constants in headers.**

```cpp
// ✅ GOOD: Compile-time constant
inline constexpr size_t BOARD_SIZE = 9;
inline constexpr int MAX_VALUE = 9;
inline constexpr int MIN_VALUE = 1;
```

```cpp
// ❌ BAD: Magic numbers
for (int i = 0; i < 9; ++i) {
    for (int j = 0; j < 9; ++j) {
        if (board[i][j] >= 1 && board[i][j] <= 9) {
            // ...
        }
    }
}
```

---

## [[nodiscard]] Mandate

**CRITICAL: Use `[[nodiscard]]` for ALL non-void return values.**

### Why [[nodiscard]]?

Prevents accidental ignoring of important return values, especially critical for:
- `std::expected` (error handling)
- `std::optional` (value presence)
- Factory methods (resource creation)
- Validation functions (success/failure)

**Compiler error if return value is discarded → catches bugs at compile time.**

---

### Examples

```cpp
// ✅ GOOD: All non-void returns marked
[[nodiscard]] std::expected<SavedGame, SaveError> loadGame(const std::string& id) const override;
[[nodiscard]] std::optional<Position> findEmptyPosition(const Board& board) const;
[[nodiscard]] std::unique_ptr<Command> createCommand(CommandType type) const;
[[nodiscard]] bool isValid(const Board& board) const;
[[nodiscard]] int getValue(size_t row, size_t col) const;
[[nodiscard]] std::string toString() const;
```

```cpp
// ❌ BAD: Missing [[nodiscard]] (potential bugs)
std::expected<SavedGame, SaveError> loadGame(const std::string& id) const;
// Caller can accidentally ignore error:
loadGame("save_001");  // No compile error, error silently ignored!
```

**Exception:** `void` functions don't need `[[nodiscard]]`
```cpp
void setDifficulty(Difficulty difficulty);  // No return value
void clear();  // No return value
```

---

### [[nodiscard]] Rationale

**Case Study: Save Manager**
```cpp
// Without [[nodiscard]]
saveManager->save(game);  // Oops, forgot to check result
// Save silently failed, user thinks game is saved

// With [[nodiscard]]
[[nodiscard]] std::expected<void, SaveError> save(const SavedGame& game) const;

saveManager->save(game);  // ❌ Compiler error: "ignoring return value"
auto result = saveManager->save(game);  // ✅ Forces error checking
if (!result.has_value()) {
    showError("Failed to save game");
}
```

---

## Const Correctness

### 1. Mark Methods `const` When They Don't Modify State

```cpp
class GameValidator {
public:
    // ✅ GOOD: Methods don't modify state → const
    [[nodiscard]] bool isValidMove(const Board& board, size_t row, size_t col, int value) const;
    [[nodiscard]] bool isBoardComplete(const Board& board) const;

private:
    // No mutable state (stateless validator)
};
```

```cpp
// ❌ BAD: Missing const (prevents usage with const objects)
bool isValidMove(const Board& board, size_t row, size_t col, int value);  // Not const

const GameValidator validator;
validator.isValidMove(...);  // ❌ Compile error: non-const method on const object
```

---

### 2. Use `const&` for Read-Only Parameters

**Pass by `const&` for non-trivial types to avoid unnecessary copies.**

```cpp
// ✅ GOOD: Pass by const& for vectors, strings, custom types
bool isValid(const std::vector<std::vector<int>>& board) const;
void setName(const std::string& name);
void processSave(const SavedGame& game);
```

```cpp
// ❌ BAD: Pass by value causes expensive copy
bool isValid(std::vector<std::vector<int>> board) const;  // Copies entire board!
void setName(std::string name);  // Copies string
```

**Exception:** Pass by value for primitive types (`int`, `size_t`, `bool`, `enum`)
```cpp
void setDifficulty(Difficulty difficulty);  // ✅ GOOD: enum is cheap to copy
void setValue(int value);  // ✅ GOOD: int is cheap to copy
```

---

### 3. Use `mutable` for Caches and Mutexes

**Use `mutable` for members that don't affect logical const-ness.**

```cpp
class PuzzleGenerator {
public:
    [[nodiscard]] Board generate(Difficulty difficulty) const {
        // Cache lookup is const operation (doesn't change logical state)
        if (auto cached = cache_.find(difficulty); cached != cache_.end()) {
            return cached->second;
        }
        auto board = generateInternal(difficulty);
        cache_[difficulty] = board;  // Modify mutable cache
        return board;
    }

private:
    mutable std::unordered_map<Difficulty, Board> cache_;  // mutable cache
};
```

---

## Type Safety

### 1. Use `size_t` for Indices, Dimensions, and Sizes

**`size_t` is always non-negative and matches container sizes.**

```cpp
// ✅ GOOD: Consistent types, no casts needed
bool hasRowConflict(const std::vector<std::vector<int>>& board,
                    size_t row, size_t col, int value) const {
    for (size_t i = 0; i < BOARD_SIZE; ++i) {
        if (i != col && board[row][i] == value) {
            return true;
        }
    }
    return false;
}
```

```cpp
// ❌ BAD: Mixed types requiring casts
bool hasRowConflict(const std::vector<std::vector<int>>& board,
                    int row, int col, int value) const {
    for (size_t i = 0; i < BOARD_SIZE; ++i) {
        if (static_cast<int>(i) != col && board[row][i] == value) {  // Unnecessary cast!
            return true;
        }
    }
    return false;
}
```

**Rule:** `size_t` for indices/sizes, `int` for domain values (Sudoku numbers 0-9)

---

### 2. Use Semantic Types for Domain Values

```cpp
// ✅ GOOD: Semantic types communicate intent
using Board = std::vector<std::vector<int>>;  // 2D grid of Sudoku values (0-9)
using Position = struct { size_t row; size_t col; };  // Board position

enum class Difficulty { Easy, Medium, Hard, Expert };  // Puzzle difficulty
enum class SaveError { FileNotFound, DecryptionFailed, ParseError };  // Error types
enum class RegionType : int { Row = 0, Col = 1, Box = 2, None = -1 };  // Sudoku region
```

```cpp
// ❌ BAD: Raw int with implicit meaning
int region_type = 0;  // 0=row? 1=col? Who knows?

// ✅ GOOD: Typed enum with self-documenting values
RegionType region_type = RegionType::Row;
```

---

### 3. Avoid Unnecessary Type Casts

**Type casts indicate type mismatch. Fix the types instead.**

```cpp
// ✅ GOOD: No casts needed
for (size_t i = 0; i < vec.size(); ++i) {
    process(vec[i], i);  // size_t index
}
```

```cpp
// ❌ BAD: Unnecessary casts (wrong type choice)
for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
    process(vec[static_cast<size_t>(i)], i);  // Cast back and forth
}
```

---

## DRY Principle (Don't Repeat Yourself)

**Zero tolerance for code duplication. Always extract repeated logic.**

### 1. Named Constants

```cpp
// ✅ GOOD: Single source of truth (include/core/constants.h)
inline constexpr size_t BOARD_SIZE = 9;
inline constexpr int MAX_VALUE = 9;
inline constexpr int MIN_VALUE = 1;
inline constexpr size_t BOX_SIZE = 3;
inline constexpr int EMPTY_CELL = 0;
inline constexpr size_t TOTAL_CELLS = 81;

for (size_t i = 0; i < BOARD_SIZE; ++i) {
    if (value >= MIN_VALUE && value <= MAX_VALUE) {
        // ...
    }
}
```

```cpp
// ❌ BAD: Magic numbers scattered
for (int i = 0; i < 9; ++i) {
    if (value >= 1 && value <= 9) {
        // ...
    }
}
```

**Bit manipulation helpers:**

```cpp
// ✅ GOOD: Named helper (include/core/constants.h)
uint16_t mask = valueToBit(value);  // Creates bitmask for Sudoku value 1-9

// ❌ BAD: Raw bit-shift with unclear intent
uint16_t mask = static_cast<uint16_t>(1 << value);
```

---

### 2. Helper Functions

```cpp
// ✅ GOOD: Reusable helper for common operation
[[nodiscard]] std::vector<std::vector<int>> extractNumbers(const Board& board) const {
    std::vector<std::vector<int>> numbers;
    for (const auto& row : board) {
        std::vector<int> rowNums;
        for (const auto& cell : row) {
            rowNums.push_back(cell.value);
        }
        numbers.push_back(rowNums);
    }
    return numbers;
}
```

```cpp
// ❌ BAD: Duplicating nested loops in multiple locations
// Location 1
for (const auto& row : board) {
    for (const auto& cell : row) {
        process(cell.value);
    }
}

// Location 2 (exact duplicate)
for (const auto& row : board) {
    for (const auto& cell : row) {
        validate(cell.value);
    }
}
```

---

### 3. Enum-to-String Converters

```cpp
// ✅ GOOD: Single conversion function
[[nodiscard]] const char* difficultyToString(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy: return "Easy";
        case Difficulty::Medium: return "Medium";
        case Difficulty::Hard: return "Hard";
        case Difficulty::Expert: return "Expert";
    }
    return "Unknown";
}
```

```cpp
// ❌ BAD: Repeated ternary chains everywhere
const char* str = difficulty == Easy ? "Easy" :
                  difficulty == Medium ? "Medium" :
                  difficulty == Hard ? "Hard" :
                  "Expert";
```

---

### 4. Constants Namespaces

**Group related constants for organization.**

```cpp
namespace UIConstants {
    inline constexpr float BOARD_MAX_SIZE = 720.0F;
    inline constexpr float CELL_PADDING = 2.0F;
    inline constexpr float GRID_LINE_THICKNESS = 2.0F;

    namespace Colors {
        inline constexpr float BG_R = 0.98F;  // #FAF8F0
        inline constexpr float BG_G = 0.97F;
        inline constexpr float BG_B = 0.94F;
    }
}
```

---

## Memory Safety

### 1. Prefer `std::string` Over `char[]` Buffers

```cpp
// ✅ GOOD: Memory-safe, no buffer overflow risk
class SaveDialog {
    std::string saveNameBuffer_;

public:
    [[nodiscard]] const std::string& saveName() const { return saveNameBuffer_; }
};
```

```cpp
// ❌ BAD: Vulnerable to buffer overflow
class SaveDialog {
    char saveNameBuffer_[256];

public:
    void getSaveName(char* out) {
        std::strcpy(out, saveNameBuffer_);  // ❌ No bounds checking
    }
};
```

---

### 2. Use RAII for Resource Management

**Resources (files, memory, locks) managed by objects → automatic cleanup.**

```cpp
// ✅ GOOD: RAII - file automatically closed
class FileReader {
    std::ifstream file_;

public:
    explicit FileReader(const std::string& path) : file_(path) {}
    // Destructor automatically closes file
};
```

```cpp
// ❌ BAD: Manual resource management (leak risk)
void processFile(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) return;

    // Process file...
    if (error) {
        return;  // ❌ Forgot to close file!
    }

    fclose(file);
}
```

---

### 3. Smart Pointers Over Raw Pointers

```cpp
// ✅ GOOD: Smart pointers manage lifetime
class GameViewModel {
    std::shared_ptr<IGameValidator> validator_;
    std::unique_ptr<CommandHistory> history_;

public:
    GameViewModel(std::shared_ptr<IGameValidator> validator)
        : validator_(std::move(validator)),
          history_(std::make_unique<CommandHistory>()) {}
    // Automatic cleanup, no manual delete
};
```

```cpp
// ❌ BAD: Raw pointers (leak risk)
class GameViewModel {
    IGameValidator* validator_;
    CommandHistory* history_;

public:
    GameViewModel(IGameValidator* validator)
        : validator_(validator),
          history_(new CommandHistory()) {}

    ~GameViewModel() {
        delete history_;  // Easy to forget
        // validator_ not deleted → who owns it?
    }
};
```

---

### 4. No Manual `new`/`delete`

```cpp
// ✅ GOOD: Use std::make_unique, std::make_shared
auto command = std::make_unique<SetNumberCommand>(row, col, value);
auto validator = std::make_shared<GameValidator>();
```

```cpp
// ❌ BAD: Manual new/delete
Command* command = new SetNumberCommand(row, col, value);
// ... use command ...
delete command;  // Easy to forget, exception-unsafe
```

---

## Code Smells Checklist

**Zero tolerance for these anti-patterns:**

### ❌ **No `const_cast`**
Fix const-correctness instead of casting away const.

```cpp
// ❌ BAD: Casting away const
void modify(const GameState& state) {
    auto& mutableState = const_cast<GameState&>(state);
    mutableState.value = 42;
}

// ✅ GOOD: Fix the signature
void modify(GameState& state) {
    state.value = 42;
}
```

---

### ❌ **No Sentinel Values**
Use `std::optional` or `std::expected`.

```cpp
// ❌ BAD: -1 means "not found"
int findIndex(const std::vector<int>& vec, int target) {
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] == target) return static_cast<int>(i);
    }
    return -1;  // Magic sentinel
}

// ✅ GOOD: std::optional
[[nodiscard]] std::optional<size_t> findIndex(const std::vector<int>& vec, int target) {
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] == target) return i;
    }
    return std::nullopt;
}
```

---

### ❌ **No Magic Numbers**
Use named constants.

```cpp
// ❌ BAD
if (age >= 18 && age <= 65) { /* ... */ }

// ✅ GOOD
inline constexpr int ADULT_AGE = 18;
inline constexpr int RETIREMENT_AGE = 65;
if (age >= ADULT_AGE && age <= RETIREMENT_AGE) { /* ... */ }
```

---

### ❌ **No Long Functions**
Extract helper methods. Max ~50 lines per function.

```cpp
// ❌ BAD: 200-line function
void processGame() {
    // 200 lines of logic
}

// ✅ GOOD: Break into smaller functions
void processGame() {
    validateInput();
    updateState();
    renderUI();
}
```

---

### ❌ **No Duplicated Code**
Apply DRY principle. Extract common logic.

---

### ❌ **No Missing [[nodiscard]]**
All non-void return values must be marked.

---

### ❌ **No Deleting Unused Code Without Asking**
If a variable appears unused, use `[[maybe_unused]]` instead of deleting.
```cpp
// ✅ GOOD: Mark as intentionally unused
[[maybe_unused]] const auto scalarResult = computeScalar();
// Code may be kept for future use (e.g., scalar fallback for SIMD)
```

---

### ❌ **No Making Interface Methods Static**
Never convert virtual/interface methods to static.
```cpp
// ❌ BAD: Breaking dependency injection
class IValidator {
    static bool validate();  // Can't override, breaks polymorphism
};

// ✅ GOOD: Keep as virtual instance method
class IValidator {
    virtual bool validate() const = 0;  // Overridable, testable with mocks
};
```

---

## AAA Style Reference

**Almost Always Auto (AAA):** Use `auto` for local variables where type is clear from context.

**See [AAA_STYLE_GUIDE.md](AAA_STYLE_GUIDE.md) for comprehensive guidance.**

**Quick Reference:**
```cpp
// ✅ GOOD: Type obvious from initialization
auto validator = std::make_shared<GameValidator>();
auto result = saveManager->load("save_001");
auto [row, col] = findEmptyCell(board);

// ❌ BAD: Type unclear, spell it out
auto value = compute();  // What type is this?
GameValidator validator = GameValidator();  // Redundant type name
```

---

## See Also

- [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md) - MVVM, SOLID, DIP
- [AAA_STYLE_GUIDE.md](AAA_STYLE_GUIDE.md) - Almost Always Auto style
- [TDD_GUIDE.md](TDD_GUIDE.md) - Test-driven development
- [CODE_QUALITY.md](CODE_QUALITY.md) - Formatting, static analysis
- [DEBUGGING_GUIDE.md](DEBUGGING_GUIDE.md) - Debug methodology
