# Architecture Principles

This document provides a deep dive into the architectural design principles that guide the sudoku-cpp codebase. These principles ensure maintainability, testability, and separation of concerns.

---

## Table of Contents

- [MVVM Pattern (Model-View-ViewModel)](#mvvm-pattern-model-view-viewmodel)
  - [Board Data Types](#board-data-types)
- [SOLID Principles](#solid-principles)
- [Dependency Injection](#dependency-injection)
- [Single-Threaded Architecture](#single-threaded-architecture)
- [Observable Pattern](#observable-pattern)
- [ITimeProvider Pattern](#itimeprovider-pattern)
- [Architectural Decision Records](#architectural-decision-records)

---

## MVVM Pattern (Model-View-ViewModel)

The codebase strictly adheres to the **MVVM (Model-View-ViewModel)** pattern for clear separation of concerns across three layers:

### Layer Responsibilities

**1. Model Layer (Business Logic)**
- **Location:** `src/core/`
- **Components:** `GameValidator`, `PuzzleGenerator`, `SaveManager`, `StatisticsManager`
- **Responsibility:** Implements core business logic and domain rules
- **No knowledge of:** UI rendering, presentation logic, user interactions

**2. ViewModel Layer (Presentation Logic)**
- **Location:** `src/view_model/`
- **Components:** `GameViewModel`
- **Responsibility:** Orchestrates Model services, exposes Observable state for UI binding
- **Pattern:** Observable pattern for automatic UI updates
- **No knowledge of:** UI rendering details, Qt widget specifics

**3. View Layer (UI Rendering)**
- **Location:** `src/view/`
- **Components:** `MainWindow` (Qt6 Widgets)
- **Responsibility:** Renders UI and handles user input
- **No knowledge of:** Business logic, domain rules, data persistence

### Data Flow

```
User Action → View → ViewModel → Model
             ↓
Model State Change → ViewModel (Observable) → View (auto-update)
```

**Example:**
1. User clicks "New Game" button in `MainWindow` (View)
2. View calls `gameViewModel->newGame(difficulty)` (ViewModel)
3. ViewModel calls `puzzleGenerator->generate(difficulty)` (Model)
4. Model returns new puzzle, ViewModel updates `Observable<GameState>`
5. View automatically re-renders (subscribed to Observable)

### Benefits

- **Testability:** ViewModel and Model can be tested without UI
- **Maintainability:** Clear boundaries prevent spaghetti code
- **Reusability:** Model layer can be used in different UIs (CLI, web, mobile)

### Board Data Types

The codebase uses distinct board representations optimized for each architectural layer:

```
View / ViewModel                  Model (Core)                    Solver (Internal)
┌─────────────────┐    extract    ┌───────────────┐   fromBoard   ┌──────────────────┐
│  GameState       │───Numbers()──▶│  BoardData     │───Data()────▶│  Board            │
│  (flat arrays)   │              │  int[81]       │              │  int8_t[96]       │
│  values/notes/   │              │                │              │  alignas(32)      │
│  flags           │◀─loadPuzzle──│                │◀─toBoardData─│                   │
└─────────────────┘    ()         └───────────────┘    ()          └──────────────────┘
      │                                │                                │
      │ BoardData values_              │ strategies,                    │ backtracking,
      │ NotesData notes_data_          │ validation,                    │ SIMD solution
      │ CellFlags givens_/             │ save/load                      │ counting
      │   hints_/conflicts_/           │                                │
      │   highlights_                  │                                │
      │                           ┌────┴───────────┐            ┌──────┴──────────┐
      │                           │ CandidateGrid   │            │ ConstraintState  │
      │                           │ uint16_t[81]    │            │ uint16_t[27]     │
      │                           │ + ConstraintState│            │ row/col/box masks│
      │                           └────────────────┘            └─────────────────┘
```

**Type details:**

| Type | Storage | Size | Location | Purpose |
| ------ | --------- | ------ | ---------- | --------- |
| `GameState` | `BoardData` + `NotesData` + 4x `CellFlags` | ~530 B | `src/model/game_state.h` | UI state: values, notes, given/hint/conflict/highlight flags |
| `BoardData` | `int[81]` flat array | 324 B | `include/core/board_data.h` | Public API boundary: strategies, validation, serialization |
| `CellFlags` | `std::bitset<81>` | 11 B | `include/core/board_data.h` | Per-cell boolean flags (givens, hints, conflicts, highlights) |
| `Board` | `int8_t[96]` aligned | 96 B | `include/core/board.h` | SIMD-optimized backtracking and solution counting |
| `CandidateGrid` | `uint16_t[81]` + `ConstraintState` | 216 B | `src/core/candidate_grid.h` | Per-cell candidate tracking for solving strategies |
| `ConstraintState` | `uint16_t[27]` | 54 B | `src/core/constraint_state.h` | Row/column/box constraint bitmasks for O(1) candidate queries |

**Why separate board types?**

1. **`GameState`** stores rich per-cell metadata (notes, flags, conflict markers) needed by the ViewModel and View layers, using flat arrays (`BoardData`, `NotesData`, `CellFlags`) for zero heap allocations and cache locality. The `Cell` struct is a read-only snapshot assembled on demand via `getCell()`.

2. **`BoardData`** is the public API type used at all interface boundaries (`IGameValidator`, `ISolvingStrategy`, `ISaveManager`). Uses `int` for simplicity and `std::span<int, 9>` row accessors for ergonomic `board[row][col]` syntax.

3. **`Board`** is an internal type used only by the backtracking solver and solution counter. Uses `int8_t` to fit more cells per SIMD register and `alignas(32)` for AVX2 aligned loads. The 15 bytes of padding (cells 81-95, always zero) avoid bounds checks in SIMD loops.

**Conversion points:**

- `GameState::extractNumbers()` — returns the internal `BoardData values_` directly (zero-cost copy)
- `Board::fromBoardData()` / `Board::toBoardData()` — BoardData to/from Board (int narrowing/widening)
- `CandidateGrid` constructor takes `BoardData` and builds constraint masks automatically

**Design rule:** Layer boundaries always use `BoardData`. Internal solver code may use `Board` or `CandidateGrid` for performance, but these never appear in public interfaces.

---

## SOLID Principles

The codebase is designed around SOLID principles with extensive use of interfaces and dependency injection.

### 1. Single Responsibility Principle (SRP)

**Each class has one clearly defined responsibility.**

**Examples:**
- `GameValidator`: **Only** validates Sudoku rules
- `PuzzleGenerator`: **Only** generates puzzles
- `SaveManager`: **Only** handles save/load persistence
- `StatisticsManager`: **Only** tracks game statistics

**Anti-Pattern (Avoided):**
```cpp
// ❌ BAD: God class with multiple responsibilities
class Game {
    void validateMove();
    void generatePuzzle();
    void saveToDisk();
    void calculateStatistics();
    void renderUI();
};
```

**Good Design (Current):**
```cpp
// ✅ GOOD: Separate interfaces for distinct concerns
class IGameValidator { virtual bool isValidMove(...) = 0; };
class IPuzzleGenerator { virtual Board generate(...) = 0; };
class ISaveManager { virtual void save(...) = 0; };
class IStatisticsManager { virtual void recordCompletion(...) = 0; };
```

---

### 2. Open/Closed Principle (OCP)

**Classes are open for extension but closed for modification.**

**Example: Difficulty Levels**
New difficulty levels can be added without modifying existing validation logic:

```cpp
enum class Difficulty { Easy, Medium, Hard, Expert };

// Adding 'Expert' doesn't require changing GameValidator
// Just add case to PuzzleGenerator's clue count table
```

**Example: Command Pattern (Undo/Redo)**
New command types can be added without modifying `CommandHistory`:

```cpp
class Command { virtual void execute() = 0; virtual void undo() = 0; };
class SetNumberCommand : public Command { /* ... */ };
class SetNoteCommand : public Command { /* ... */ };
// Add new commands without changing CommandHistory
```

---

### 3. Liskov Substitution Principle (LSP)

**All implementations are substitutable for their interfaces.**

**Example:**
```cpp
// Production code
auto validator = std::make_shared<GameValidator>();
auto viewModel = std::make_shared<GameViewModel>(validator, ...);

// Test code - substitute with mock
auto mockValidator = std::make_shared<MockGameValidator>();
auto viewModel = std::make_shared<GameViewModel>(mockValidator, ...);

// viewModel behaves correctly with either implementation
```

**Contract Guarantee:**
- `IGameValidator::isValidMove()` always returns the same result for the same input
- `ISaveManager::load()` always returns `std::expected<SavedGame, SaveError>`
- No implementation breaks the contract defined by its interface

---

### 4. Interface Segregation Principle (ISP)

**Clients depend only on interfaces they use.**

**Example:**
```cpp
// ✅ GOOD: Focused interfaces
class IGameValidator {
    virtual bool isValidMove(...) const = 0;
    virtual bool isBoardComplete(...) const = 0;
};

class ISaveManager {
    virtual std::expected<SavedGame, SaveError> load(...) const = 0;
    virtual std::expected<void, SaveError> save(...) const = 0;
};

// View layer doesn't depend on ISaveManager (doesn't need it)
// ViewModel doesn't depend on rendering (doesn't need it)
```

**Anti-Pattern (Avoided):**
```cpp
// ❌ BAD: Monolithic interface forces clients to depend on unused methods
class IGameEngine {
    virtual void validateMove() = 0;
    virtual void generatePuzzle() = 0;
    virtual void saveToDisk() = 0;
    virtual void renderUI() = 0;  // View needs this, but ViewModel doesn't
    virtual void playSound() = 0;  // Audio system needs this, but others don't
};
```

---

### 5. Dependency Inversion Principle (DIP)

**High-level modules depend on abstractions, not concrete implementations.**

**Example: GameViewModel Dependencies**
```cpp
class GameViewModel {
public:
    GameViewModel(
        std::shared_ptr<IGameValidator> validator,      // ← Interface, not GameValidator
        std::shared_ptr<IPuzzleGenerator> generator,    // ← Interface, not PuzzleGenerator
        std::shared_ptr<ISaveManager> saveManager,      // ← Interface, not SaveManager
        std::shared_ptr<IStatisticsManager> statsManager // ← Interface, not StatisticsManager
    );
};
```

**Dependency Injection Container:**
The `DIContainer` manages all dependencies centrally:

```cpp
DIContainer container;
container.registerSingleton<IGameValidator>(std::make_shared<GameValidator>());
container.registerSingleton<IPuzzleGenerator>(std::make_shared<PuzzleGenerator>());
// ... register all services

auto viewModel = container.resolve<GameViewModel>();  // Dependencies auto-injected
```

---

## Dependency Injection

**All dependencies are injected via constructor parameters.**

### Benefits

1. **Testability:** Replace real implementations with mocks in tests
2. **Flexibility:** Swap implementations without changing client code
3. **Explicit Dependencies:** Constructor signature shows what class needs
4. **Lifetime Management:** DI container controls object lifetimes

### Pattern: Constructor Injection

```cpp
// ✅ GOOD: Dependencies explicit in constructor
class GameViewModel {
    std::shared_ptr<IGameValidator> validator_;
    std::shared_ptr<IPuzzleGenerator> generator_;

public:
    GameViewModel(
        std::shared_ptr<IGameValidator> validator,
        std::shared_ptr<IPuzzleGenerator> generator
    ) : validator_(validator), generator_(generator) {}
};
```

**Anti-Pattern (Avoided):**
```cpp
// ❌ BAD: Hard-coded dependencies
class GameViewModel {
    GameValidator validator_;  // Can't substitute in tests
    PuzzleGenerator generator_;

public:
    GameViewModel() : validator_(), generator_() {}  // Hidden dependencies
};
```

### DI Container Usage

The `DIContainer` class provides centralized dependency management:

```cpp
// Registration (in main.cpp)
container.registerSingleton<IGameValidator>(std::make_shared<GameValidator>());
container.registerFactory<IPuzzleGenerator>([]() {
    return std::make_shared<PuzzleGenerator>();
});

// Resolution
auto validator = container.resolve<IGameValidator>();  // Returns singleton
auto generator = container.resolve<IPuzzleGenerator>();  // Creates new instance
```

---

## Single-Threaded Architecture

**All code runs on the main thread. No multithreading.**

### Rationale

1. **Qt Requirement:** Qt GUI operations must run on the main thread
2. **Simplicity:** No mutexes, no race conditions, no deadlocks
3. **Debugging:** Easier to reason about state and debug issues

### Constraints

- **No `std::thread`**, `std::async`, `std::future`
- **No mutex-based synchronization** (not needed)
- **No thread pools** or background workers

### Performance Considerations

**Acceptable:** Puzzle generation is the slowest operation:
- Easy: 0ms (instant)
- Medium: 9ms
- Hard: 239ms

239ms worst-case is acceptable for an interactive game (< 1/4 second).

**Trade-off:** Can't parallelize puzzle generation, but eliminates concurrency complexity.

---

## Observable Pattern

The ViewModel exposes `Observable<T>` for state changes, enabling automatic UI updates.

### How It Works

```cpp
// ViewModel exposes Observable
class GameViewModel {
    Observable<GameState> gameState_;

public:
    const Observable<GameState>& gameState() const { return gameState_; }

    void newGame() {
        gameState_.update([](GameState& state) {
            state.board = generateNewBoard();
        });
        // Automatically notifies subscribers
    }
};

// View subscribes to Observable
class MainWindow {
    void initialize(std::shared_ptr<GameViewModel> viewModel) {
        viewModel->gameState().subscribe([this](const GameState& state) {
            // Auto-called when state changes
            this->renderBoard(state.board);
        });
    }
};
```

### Pattern Guidelines

- **Modification:** Use `update()` to modify state (triggers notifications)
- **Read-only access:** Use `get()` to read current state without triggering notifications
- **Subscribers:** View subscribes to ViewModel's Observables, not vice versa

**See [OBSERVABLE_PATTERN.md](OBSERVABLE_PATTERN.md) for detailed usage patterns.**

---

## ITimeProvider Pattern

**All time-dependent code uses dependency-injected `ITimeProvider` interface.**

### Problem Solved

Before ITimeProvider:
- Tests used `std::this_thread::sleep_for()` → **10-20% flaky under CI load**
- Timing-dependent tests unreliable
- Hard to test time-based features deterministically

After ITimeProvider:
- Tests use `MockTimeProvider` with `advanceTime()` → **100% reliable**
- Zero timing-dependent failures since adoption
- Deterministic testing for timers, auto-save, statistics

### Usage

```cpp
// Production code
class GameState {
    std::shared_ptr<ITimeProvider> timeProvider_;

public:
    GameState(std::shared_ptr<ITimeProvider> timeProvider)
        : timeProvider_(timeProvider) {}

    void startTimer() {
        startTime_ = timeProvider_->now();
    }

    auto elapsedTime() const {
        return timeProvider_->now() - startTime_;
    }
};

// Test code
auto mockTime = std::make_shared<MockTimeProvider>();
GameState state(mockTime);

state.startTimer();
mockTime->advanceTime(std::chrono::minutes(5));  // Simulate 5 minutes instantly
REQUIRE(state.elapsedTime() == std::chrono::minutes(5));
```

**See [ITIMEPROVIDER_PATTERN.md](ITIMEPROVIDER_PATTERN.md) for comprehensive usage patterns and examples.**

---

## Architectural Decision Records

### Why MVVM Pattern?

**Problem:** UI state synchronization is complex without clear separation. Manual UI update calls are error-prone (easy to forget).

**Solution:** MVVM with Observable pattern. ViewModel exposes `Observable<GameState>`, View subscribes for automatic updates.

**Trade-offs:**
- **Pro:** Zero UI state desync bugs since adoption (January 2025)
- **Pro:** Testable presentation logic without UI dependencies
- **Con:** More boilerplate (Observable wrappers, subscriptions)

**Impact:** Development velocity increased (fewer UI bugs), test coverage improved (ViewModel fully testable).

---

### Why Single-Threaded Architecture?

**Requirement:** Qt GUI operations require main thread execution.

**Benefits:**
- Eliminates race conditions, deadlocks, memory barriers
- Simplifies debugging (linear control flow)
- Reduces cognitive load (no thread safety reasoning)

**Trade-off:** Can't parallelize puzzle generation.
- **Acceptable:** 239ms worst-case (hard difficulty) is < 1/4 second
- **User experience:** Imperceptible delay for interactive game

**Alternative Rejected:** Background puzzle generation with thread pool.
- **Reason:** Adds complexity (mutexes, condition variables) for minimal benefit (0.2s savings)
- **Decision:** Simplicity > micro-optimization

---

### Why ITimeProvider Dependency Injection?

**Problem:** Time-dependent tests were flaky (10-20% failure rate under CI load).
- `std::this_thread::sleep_for(50ms)` unreliable under system load
- CI builds would randomly fail on timing assertions

**Solution:** Inject `ITimeProvider` interface.
- Production: `SystemTimeProvider` uses real clock
- Tests: `MockTimeProvider` with `advanceTime()` for deterministic time control

**Impact:**
- **Before:** 10-20% flaky test rate
- **After:** 100% test reliability, zero timing-dependent failures

**Alternative Rejected:** `std::this_thread::sleep_for()` in tests.
- **Reason:** Unreliable under system load (CI runners, resource contention)
- **Decision:** Determinism > real-time testing

---

### Why Observable Pattern Over Manual UI Updates?

**Problem:** Manually calling UI update methods error-prone.
```cpp
// ❌ BAD: Easy to forget update call
void GameViewModel::setDifficulty(Difficulty diff) {
    difficulty_ = diff;
    // Oops! Forgot to notify UI → stale state
}
```

**Solution:** Observable automatically notifies subscribers on state changes.
```cpp
// ✅ GOOD: update() guarantees notification
void GameViewModel::setDifficulty(Difficulty diff) {
    difficulty_.update([diff](Difficulty& d) { d = diff; });
    // Subscribers auto-notified
}
```

**Trade-off:**
- **Pro:** Guarantees UI consistency (impossible to forget notification)
- **Pro:** Decouples ViewModel from View (ViewModel doesn't know about UI)
- **Con:** Slight complexity in ViewModel (Observable wrappers)

**Decision:** Reliability > simplicity. Zero UI desync bugs worth the Observable overhead.

---

## Architecture Overview Summary

**Technology Stack:**
- C++23 (std::expected, std::optional, designated initializers)
- Qt6 Widgets for cross-platform GUI
- CMake + Conan for build/dependency management
- Catch2 for unit/integration testing

**Architectural Layers:**
- **Model:** `GameValidator`, `PuzzleGenerator`, `SaveManager`, `StatisticsManager`, `LocalizationManager` (interfaces)
- **ViewModel:** `GameViewModel` (Observable pattern, Command pattern for undo/redo)
- **View:** `MainWindow` (Qt6 Widgets, subscribes to ViewModel Observables, localized via `ILocalizationManager`)

**Key Patterns:**
- MVVM for separation of concerns
- Dependency Injection for testability and flexibility
- Observable for reactive UI updates
- Command for undo/redo
- ITimeProvider for deterministic time testing
- ILocalizationManager for UI string externalization

**Design Principles:**
- SOLID (SRP, OCP, LSP, ISP, DIP)
- Single-threaded (Qt main thread requirement + simplicity)
- Interface-based design (depend on abstractions, not implementations)
- Test-driven development (TDD with mocks for all dependencies)

---

## See Also

- [OBSERVABLE_PATTERN.md](OBSERVABLE_PATTERN.md) - Detailed Observable usage patterns
- [ITIMEPROVIDER_PATTERN.md](ITIMEPROVIDER_PATTERN.md) - Time abstraction for testability
- [UI_LOCALIZATION.md](UI_LOCALIZATION.md) - Localization architecture and string extraction
- [TDD_GUIDE.md](TDD_GUIDE.md) - Test-driven development practices
- [DEBUGGING_GUIDE.md](DEBUGGING_GUIDE.md) - Debugging methodology and tools
- [CPP_STYLE_GUIDE.md](CPP_STYLE_GUIDE.md) - C++23 coding standards
