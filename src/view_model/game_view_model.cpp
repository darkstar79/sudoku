// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "game_view_model.h"

#include "../core/solving_technique.h"
#include "core/board_utils.h"
#include "core/constants.h"
#include "core/i18n_helpers.h"
#include "core/i_game_validator.h"
#include "core/i_puzzle_generator.h"
#include "core/i_save_manager.h"
#include "core/i_statistics_manager.h"
#include "core/i_sudoku_solver.h"
#include "core/observable.h"
#include "model/game_state.h"

#include <algorithm>
#include <chrono>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

GameViewModel::GameViewModel(
    std::shared_ptr<core::IGameValidator> validator, std::shared_ptr<core::IPuzzleGenerator> generator,
    std::shared_ptr<core::ISudokuSolver> solver, std::shared_ptr<core::IStatisticsManager> stats_manager,
    std::shared_ptr<core::ISaveManager> save_manager, std::shared_ptr<core::ISettingsManager> settings_manager,
    std::shared_ptr<core::IPuzzleAnalyzer> analyzer, std::shared_ptr<core::IClipboardProvider> clipboard)
    : gameState(model::GameState{}), uiState(UIState{}), statistics(StatsDisplay{}), errorMessage(std::string{}),
      hintMessage(std::string{}), coachingState(viewmodel::CoachingState{}), validator_(std::move(validator)),
      generator_(std::move(generator)), solver_(std::move(solver)), stats_manager_(std::move(stats_manager)),
      save_manager_(std::move(save_manager)), settings_manager_(std::move(settings_manager)),
      analyzer_(std::move(analyzer)), clipboard_(std::move(clipboard)) {
    // Apply initial settings if available
    if (settings_manager_) {
        const auto& settings = settings_manager_->getSettings();
        uiState.update([&settings](UIState& state) {
            state.show_conflicts = settings.show_conflicts;
            state.show_hints = settings.show_hints;
        });
        // Wire detailed stats settings to statistics manager
        if (stats_manager_) {
            stats_manager_->setCollectDetailedStats(settings.collect_detailed_stats);
            stats_manager_->setEncryptSessions(settings.encrypt_detailed_stats);
        }
    }
    spdlog::debug("GameViewModel initialized with dependencies");
    updateUIState();
    refreshStatistics();
}

std::string GameViewModel::statisticsErrorToString(core::StatisticsError error) const {
    switch (error) {
        case core::StatisticsError::InvalidGameData:
            return core::loc("Sudoku", "Invalid game data");
        case core::StatisticsError::FileAccessError:
            return core::loc("Sudoku", "File access error");
        case core::StatisticsError::SerializationError:
            return core::loc("Sudoku", "Serialization error");
        case core::StatisticsError::InvalidDifficulty:
            return core::loc("Sudoku", "Invalid difficulty");
        case core::StatisticsError::GameNotStarted:
            return core::loc("Sudoku", "Game not started");
        case core::StatisticsError::GameAlreadyEnded:
            return core::loc("Sudoku", "Game already ended");
        default:
            return core::loc("Sudoku", "Unknown statistics error");
    }
}

void GameViewModel::startNewGame(core::Difficulty difficulty) {
    spdlog::info("Starting new game with difficulty: {}", static_cast<int>(difficulty));

    resetCoachingState();

    // Configure puzzle generation settings
    core::GenerationSettings settings;
    settings.difficulty = difficulty;
    settings.seed = 0;  // Random seed
    // MCV heuristic optimization enables production use of unique solution validation
    // Performance: Easy ~200ms, Medium ~800ms, Hard ~3s, Minimal ~10s
    settings.ensure_unique = true;
    settings.max_attempts = 1000;

    auto puzzle_result = generator_->generatePuzzle(settings);
    if (!puzzle_result) {
        handleError(core::loc("Sudoku", "Failed to generate puzzle"));
        return;
    }

    // Store puzzle rating and techniques for statistics tracking and display
    current_puzzle_rating_ = puzzle_result->rating;
    current_puzzle_techniques_ = puzzle_result->required_techniques;
    current_puzzle_requires_backtracking_ = puzzle_result->requires_backtracking;
    // Freshly rated by this build → stamp the current rating-model version (0b.0 firewall).
    current_puzzle_rating_model_version_ = core::RATING_MODEL_VERSION;
    current_puzzle_origin_ = core::PuzzleOrigin::Generated;

    // Create new game state
    model::GameState new_state;
    new_state.loadPuzzle(puzzle_result->board);
    new_state.setDifficulty(difficulty);
    new_state.setSolutionBoard(puzzle_result->solution);  // Store solution for hints

    gameState.set(new_state);

    // Start timer via update() to ensure Observable internal state has timer_running_ = true
    gameState.update([](model::GameState& state) { state.startTimer(); });

    // Clear move history
    resetMoveHistory();

    // Start statistics session
    startGameSession();

    updateUIState();
    spdlog::info("New game started successfully");
}

void GameViewModel::resetGame() {
    if (!isGameActive()) {
        return;  // No active game to reset
    }

    spdlog::info("Resetting current game to original puzzle state");

    // Capture current puzzle data before resetting.
    //
    // Story 8.16 / D1: the solution is read through trySolutionBoard() and copied only when one
    // exists. A game restored from a save or built in custom-puzzle edit mode carries no solution
    // board (the save format has no solution field; commitEditedPuzzle never runs the solver), and
    // the unguarded read here used to throw std::bad_optional_access straight out of a Qt slot.
    // Reset is gated on isGameActive() — merely "the timer is running" — which is true for a
    // resumed game, so the control was enabled and the crash sat on the default launch path.
    //
    // Deliberately NOT recomputed when absent: solving here would put a solver run on the reset
    // path and would flip hasSolution() semantics that story 6.8's hasPuzzle() work separated.
    // Solution-less games stay solution-less, and enterNumber's conflict-only fallback keeps
    // mistake detection working for them.
    const auto& current_state = gameState.get();
    auto original_puzzle = current_state.extractGivenNumbers();
    auto solution = current_state.trySolutionBoard();
    auto difficulty = current_state.getDifficulty();

    // End current stats session as abandoned (not completed)
    endGameSession(false);

    // Create fresh game state with original puzzle
    model::GameState new_state;
    new_state.loadPuzzle(original_puzzle);
    new_state.setDifficulty(difficulty);
    if (solution.has_value()) {
        new_state.setSolutionBoard(*solution);
    }

    gameState.set(new_state);

    // Start timer via update() to ensure Observable internal state has timer_running_ = true
    gameState.update([](model::GameState& state) { state.startTimer(); });

    // Clear move history
    resetMoveHistory();

    // Start fresh statistics session (same puzzle rating)
    startGameSession();

    // Clear messages and coaching state
    hintMessage.set("");
    errorMessage.set("");
    resetCoachingState();

    updateUIState();
    spdlog::info("Game reset successfully");
}

void GameViewModel::loadGame(const std::string& save_id) {
    spdlog::info("Loading game: {}", save_id);

    resetCoachingState();

    auto load_result = save_manager_->loadGame(save_id);
    if (!load_result) {
        handleError(core::loc("Sudoku", "Failed to load game"));
        return;
    }

    restoreGameState(*load_result);
    spdlog::info("Game loaded successfully");
}

void GameViewModel::applyProgressCounters(core::SavedGame& saved_game) const {
    if (current_game_session_ == 0) {
        return;  // No session (e.g. no game in progress) — leave the defaults at 0.
    }

    auto stats = stats_manager_->getGameStats(current_game_session_);
    if (!stats) {
        spdlog::warn("Could not read session progress for save: {}", statisticsErrorToString(stats.error()));
        return;
    }

    saved_game.moves_made = stats->moves_made;
    saved_game.hints_used = stats->hints_used;
    saved_game.mistakes = stats->mistakes;
}

void GameViewModel::startRestoredSession(const core::SavedGame& saved_game) {
    // Story 8.1 / SAVE-3: restore used to start no session at all, so current_game_session_ stayed
    // 0 — getHintCount() returned 0 (dead hint button), recordMove skipped statistics entirely, and
    // completion's endGameSession(true) was a no-op, i.e. a finished resumed game never counted.
    //
    // Call order matters: the caller must have restored current_puzzle_rating_ first, or the stats
    // record carries rating 0.0. This mirrors the import paths' "analyze before startGameSession"
    // ordering. The corruption-guard early returns in restoreGameState route through startNewGame,
    // which starts its own session — deliberately none is started for a rejected save.
    startGameSession();
    if (current_game_session_ == 0) {
        return;  // startGameSession can fail silently; every other stats call guards the same way.
    }

    // A resumed game continues its predecessor's progress: seed the counters and the prior play
    // time so the hint budget survives a restart and completion reports true play time.
    //
    // hints_used is seeded VERBATIM (only StatisticsManager's [0, MAX_PROGRESS_COUNTER] clamp
    // applies). Deliberately NOT clamped to the configured max_hints: the seeded value is written
    // back out by the next save (applyProgressCounters), so clamping here would let a settings
    // round-trip destroy it — spend 8 of 10 hints, lower max_hints to 3, resume, auto-save, raise
    // it back to 10, and the player is handed 7 hints instead of 2. The budget is floored at the
    // consumer instead: getHintCount() is the only reader and clamps at 0 there.
    auto seed_result =
        stats_manager_->seedSessionProgress(current_game_session_, saved_game.moves_made, saved_game.hints_used,
                                            saved_game.mistakes, saved_game.elapsed_time);
    if (!seed_result) {
        // Log and continue, matching the recordHint style: the game stays playable, only the
        // carried-over counters are lost.
        spdlog::warn("Failed to seed restored session progress: {}", statisticsErrorToString(seed_result.error()));
    }
}

bool GameViewModel::isCorruptedManualSave(const core::SavedGame& saved_game) {
    // These heuristics key off move_history (emptiness / presence) as a corruption tell. That signal
    // is only valid for MANUAL saves, which persist the full forward log. Auto-saves are deliberately
    // written WITHOUT history (SaveManager::autoSave sets include_history = false), so an empty
    // move_history is their normal state and says nothing about integrity. Applying these guards to
    // auto-saves discards every in-progress auto-save and breaks resume-on-restart entirely
    // (Story 6.5). Gate both checks on !is_auto_save so manual-save protection is preserved while
    // auto-saves with real progress resume.
    if (saved_game.is_auto_save) {
        return false;
    }

    // Detect the "all cells marked given" corruption: a bug once wrote saves in which every cell
    // was a clue, leaving a board that could not be played at all.
    //
    // The tell is that original_puzzle has NO empty cells. This check used to read "original_puzzle
    // == current_state AND there is progress (a move log or any note)", which is not that bug's
    // signature at all — it is the signature of an ordinary game in which the player has entered
    // pencil marks but not yet placed a digit. Story 8.16 / D3: pencil-mark a generated game, Save
    // Game, then load it back, and the guard fired and replaced the board with a fresh random
    // puzzle. The Load dialog even listed the save with correct metadata first, so the loss looked
    // like the app had simply started a new game. A real puzzle always leaves cells to fill, so the
    // completely-filled test catches the corruption without touching any legitimate save.
    // Same two predicates the import and edit-mode-commit paths refuse a board on, so a save can
    // only reach this state by corruption or by predating those guards.
    if (!core::hasEmptyCell(saved_game.original_puzzle)) {
        spdlog::warn("Corrupted save detected (original_puzzle has no empty cells — every cell marked given), "
                     "starting new game instead");
        return true;
    }
    // A puzzle with no clues at all is the other structurally impossible shape, and it is worse
    // than merely unplayable: it restores with an empty givens_ set, so hasPuzzle() is false, and
    // autoSave() — which gates on hasPuzzle() — then skips the game forever while the stale file is
    // re-resumed on every launch. The old check only caught this when the save also carried notes.
    if (!core::hasAnyGiven(saved_game.original_puzzle)) {
        spdlog::warn("Corrupted save detected (original_puzzle has no givens at all), starting new game instead");
        return true;
    }

    // Detect phantom-value corruption: user values exist in current_state but move_history is empty.
    // This happens when a bug placed values during startup without recording them as moves.
    //
    // history_complete gates the inference (story 8.16 / D2). An empty log only implies corruption
    // when the save claims to carry a full one. A game resumed from an auto-save legitimately has a
    // short log — auto-saves are written without history by design (story 6.5) — and every manual
    // save derived from it inherits that. Without this gate, saving a resumed game produced a file
    // whose reload silently threw the player's board away. Pre-8.16 saves have no such key and
    // default to true, so the guard still applies to them exactly as before.
    bool has_user_values = saved_game.original_puzzle != saved_game.current_state;
    if (has_user_values && saved_game.move_history.empty() && saved_game.history_complete) {
        spdlog::warn("Corrupted save detected (user values present but empty move history), "
                     "starting new game instead");
        return true;
    }

    return false;
}

void GameViewModel::restoreGameState(const core::SavedGame& saved_game) {
    if (isCorruptedManualSave(saved_game)) {
        // startNewGame begins its own statistics session — deliberately none is started for the
        // rejected save, so this path counts exactly one session (story 8.1 AC9a).
        startNewGame(saved_game.difficulty);
        return;
    }

    // Create a GameState from the saved game
    model::GameState loaded_state;

    // Load original puzzle (only clues get is_given = true)
    loaded_state.loadPuzzle(saved_game.original_puzzle);
    loaded_state.setDifficulty(saved_game.difficulty);

    // Overlay user-entered values from current_state onto non-given cells
    core::forEachCell([&](size_t row, size_t col) {
        int saved_value = saved_game.current_state[row][col];
        if (!loaded_state.isGiven(row, col) && saved_value != 0) {
            loaded_state.setValue(row, col, saved_value);
        }
    });

    // Restore notes
    if (!saved_game.notes.empty()) {
        core::forEachCell([&](size_t row, size_t col) {
            loaded_state.setNotes({.row = row, .col = col}, saved_game.notes[row][col]);
        });
    }

    // Restore hint-revealed cells
    if (!saved_game.hint_revealed_cells.empty()) {
        core::forEachCell([&](size_t row, size_t col) {
            loaded_state.setHintRevealed({.row = row, .col = col}, saved_game.hint_revealed_cells.get(row, col));
        });
    }

    gameState.set(loaded_state);

    // Re-seat the accumulated play time and mistake counter, then start the clock — all in ONE
    // update() lambda. GameState::operator== compares neither elapsed_time_ nor start_time_, so an
    // update that only re-seats the clock would not notify observers; startTimer()'s timer_running_
    // flip is what guarantees the notification here. (Do not add the timer fields to operator== —
    // that would make every tick a "change".) Story 8.1 / SAVE-2: before this, restore left
    // elapsed_time_ at 0 and the next auto-save wrote that zero over the stored value.
    gameState.update([&saved_game](model::GameState& state) {
        state.setElapsedTime(saved_game.elapsed_time);
        // Clamp the upper bound here too, not just the negative one setMistakeCount applies: this
        // counter is read straight out of an unvalidated save field (story 7-1 range-validates
        // board/move/note fields but NOT the progress counters) and is rendered verbatim in the
        // game-info dialog. Without this, a save claiming 2e9 mistakes displays 2e9.
        state.setMistakeCount(std::min(saved_game.mistakes, core::MAX_PROGRESS_COUNTER));
        state.startTimer();
    });

    // Restore move history if available
    move_history_ = saved_game.move_history;
    move_history_index_ = static_cast<int>(move_history_.size()) - 1;
    // Derive this from what the save actually CONTAINS rather than inheriting the flag it carries.
    // The question is only ever "does the log I now hold explain the board I now hold?", and both
    // are right here — an empty log is a complete account exactly when the board still equals its
    // original puzzle. Trusting the incoming flag instead gets it wrong in both directions:
    //
    //   * A pre-8.16 save has no history_complete key, so it defaults to true. A legacy auto-save
    //     carrying real progress would therefore be resumed as "fully logged", and the first manual
    //     save of that session would be rejected on reload all over again — reopening the exact D2
    //     data loss for every user's first session after upgrading, and permanently for anyone with
    //     auto-save turned off.
    //   * Every auto-save is written history_complete = false. Quitting with no moves made and
    //     relaunching would then exempt that whole session from the phantom-value guard, even
    //     though the log it builds from that board is complete.
    //
    // The persisted key still matters, but for the other half of the problem: at load time the
    // heuristic is judging a FILE, whose board and empty log look identical whether the log was
    // truncated legitimately or never written by a bug. Only the writer knew which, so only the
    // writer can say. Here we are the writer's successor and can just look.
    move_history_complete_ = !move_history_.empty() || saved_game.original_puzzle == saved_game.current_state;

    // Recalculate last valid state and update conflict highlighting
    auto board = gameState.get().extractNumbers();
    auto conflicts = validator_->findConflicts(board);
    if (conflicts.empty()) {
        // Current state is valid
        last_valid_state_index_ = move_history_index_;
    } else {
        // Current state has conflicts, mark as no valid state known
        last_valid_state_index_ = -1;
    }
    gameState.update([&conflicts](model::GameState& state) { state.updateConflicts(conflicts); });

    // Restore puzzle rating and techniques
    current_puzzle_rating_ = saved_game.puzzle_rating;
    current_puzzle_requires_backtracking_ = saved_game.puzzle_requires_backtracking;
    // Preserve the loaded save's rating-model provenance (do NOT re-stamp to current): a legacy
    // save's stored rating is a snapshot, so a later re-save must keep its older version and stay
    // recognizably stale (0b.0 firewall). No recompute happens here.
    current_puzzle_rating_model_version_ = saved_game.rating_model_version;
    current_puzzle_techniques_.clear();
    for (int id : saved_game.puzzle_technique_ids) {
        current_puzzle_techniques_.insert(static_cast<core::SolvingTechnique>(id));
    }
    current_puzzle_origin_ = saved_game.origin;

    // Must run AFTER the rating fields above are restored (see the function's contract).
    startRestoredSession(saved_game);

    updateUIState();
    spdlog::debug("Game state restored: {} moves in history", move_history_.size());
}

bool GameViewModel::saveCurrentGame(const std::string& name) {
    spdlog::info("Saving current game: {}", name.empty() ? "auto-save" : name);

    if (!isGameActive()) {
        handleError(core::loc("Sudoku", "No active game to save"));
        return false;
    }

    core::SavedGame saved_game;
    const auto& current_state = gameState.get();
    saved_game.original_puzzle = current_state.extractGivenNumbers();
    saved_game.current_state = current_state.extractNumbers();
    saved_game.difficulty = current_state.getDifficulty();
    saved_game.elapsed_time = current_state.getElapsedTime();
    saved_game.move_history = move_history_;
    saved_game.history_complete = move_history_complete_;
    saved_game.created_time = std::chrono::system_clock::now();  // determinism-ok: persisted save wall-clock metadata

    // Extract notes
    core::forEachCell([&](size_t row, size_t col) { saved_game.notes[row][col] = current_state.getNotes(row, col); });

    // Extract hint-revealed cells
    core::forEachCell([&](size_t row, size_t col) {
        saved_game.hint_revealed_cells.set(row, col, current_state.isCellHintRevealed({.row = row, .col = col}));
    });

    // Persist puzzle rating
    saved_game.puzzle_rating = current_puzzle_rating_;
    saved_game.puzzle_requires_backtracking = current_puzzle_requires_backtracking_;
    saved_game.rating_model_version = current_puzzle_rating_model_version_;
    for (const auto& tech : current_puzzle_techniques_) {
        saved_game.puzzle_technique_ids.push_back(static_cast<int>(tech));
    }
    saved_game.origin = current_puzzle_origin_;

    // Progress counters live in the statistics session (hints_used exists nowhere else). Story 8.1
    // / SAVE-2: these three keys already round-tripped through the serializer but no producer ever
    // wrote them, so every save carried zeros.
    applyProgressCounters(saved_game);

    saved_game.display_name = name;

    auto save_result = save_manager_->saveGame(saved_game, core::manualSaveSettings(name));
    if (!save_result) {
        handleError(core::loc("Sudoku", "Failed to save game"));
        return false;
    }

    spdlog::info("Game saved successfully with ID: {}", *save_result);
    return true;
}

void GameViewModel::autoSave() {
    // Persist any in-progress game, including a paused one. isGameActive() is `timerRunning`, which
    // is false while paused (story 6.8) — gating on it would silently drop the exit auto-save on a
    // pause then close. hasPuzzle() = "a puzzle is loaded" covers running AND paused games and is
    // true for restored games (which carry no solution_board_). A completed game is skipped here and
    // its auto-save is cleared elsewhere (story 6.9).
    const auto& current_state = gameState.get();
    if (auto_save_enabled_ && current_state.hasPuzzle() && !isGameComplete()) {
        core::SavedGame auto_save_game;
        auto_save_game.original_puzzle = current_state.extractGivenNumbers();
        auto_save_game.current_state = current_state.extractNumbers();
        auto_save_game.difficulty = current_state.getDifficulty();
        auto_save_game.elapsed_time = current_state.getElapsedTime();
        auto_save_game.is_auto_save = true;
        // Always false: this producer knows it is not writing a log (SaveManager::autoSave sets
        // include_history = false), so the object must not claim otherwise even before the
        // serializer's own AND catches it. Harmless for a game with no progress — the restore path
        // derives completeness from the board, not from this flag.
        auto_save_game.history_complete = false;
        auto_save_game.puzzle_rating = current_puzzle_rating_;
        auto_save_game.puzzle_requires_backtracking = current_puzzle_requires_backtracking_;
        auto_save_game.rating_model_version = current_puzzle_rating_model_version_;
        for (const auto& tech : current_puzzle_techniques_) {
            auto_save_game.puzzle_technique_ids.push_back(static_cast<int>(tech));
        }
        auto_save_game.origin = current_puzzle_origin_;
        applyProgressCounters(auto_save_game);

        // Extract notes
        core::forEachCell(
            [&](size_t row, size_t col) { auto_save_game.notes[row][col] = current_state.getNotes(row, col); });

        // Extract hint-revealed cells
        core::forEachCell([&](size_t row, size_t col) {
            auto_save_game.hint_revealed_cells.set(row, col,
                                                   current_state.isCellHintRevealed({.row = row, .col = col}));
        });

        auto auto_save_result = save_manager_->autoSave(auto_save_game);
        if (auto_save_result.has_value()) {
            // SUCCESS: Clear dirty flag after successful save
            gameState.update([](model::GameState& state) { state.clearDirty(); });
            spdlog::debug("Auto-save completed, dirty flag cleared");
        } else {
            spdlog::warn("Auto-save failed");
        }
    }
}

}  // namespace sudoku::viewmodel