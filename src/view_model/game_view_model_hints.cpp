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

#include "../core/localized_explanations.h"
#include "../core/solving_technique.h"
#include "../core/technique_descriptions.h"
#include "../core/training_hints.h"
#include "core/i18n_helpers.h"
#include "core/i_game_validator.h"
#include "core/i_statistics_manager.h"
#include "core/i_sudoku_solver.h"
#include "core/observable.h"
#include "core/solve_step.h"
#include "game_view_model.h"
#include "model/game_state.h"

#include <cassert>
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

inline constexpr int kDefaultMaxHints = 10;

namespace {

struct CoachingCheckResult {
    std::vector<core::CellHighlight> feedback;
    int correct = 0;
    int missed = 0;
    int wrong = 0;
};

CoachingCheckResult evaluatePlacementCheck(const core::SolveStep& step, const model::GameState& snapshot,
                                           const model::GameState& current_state) {
    // Solver invariant: never suggests a placement on a given cell.
    assert(!snapshot.isGiven(step.position));
    CoachingCheckResult result;
    const int current_val = current_state.getValue(step.position);
    if (current_val == step.value) {
        result.feedback.push_back({.position = step.position, .role = core::CellRole::CorrectAnswer});
        result.correct = 1;
    } else if (current_val != 0 && current_val != snapshot.getValue(step.position)) {
        result.feedback.push_back({.position = step.position, .role = core::CellRole::WrongAnswer});
        result.wrong = 1;
    } else {
        result.feedback.push_back({.position = step.position, .role = core::CellRole::MissedAnswer});
        result.missed = 1;
    }
    return result;
}

CoachingCheckResult evaluateEliminationCheck(const core::SolveStep& step, const model::GameState& snapshot,
                                             const model::GameState& current_state) {
    CoachingCheckResult result;

    // Track which expected eliminations the user correctly made
    for (const auto& elim : step.eliminations) {
        const bool snapshot_had = snapshot.getNotes(elim.position).contains(elim.value);
        const bool current_has = current_state.getNotes(elim.position).contains(elim.value);
        if (snapshot_had && !current_has) {
            result.feedback.push_back({.position = elim.position, .role = core::CellRole::CorrectAnswer});
            result.correct++;
        } else if (snapshot_had && current_has) {
            result.feedback.push_back({.position = elim.position, .role = core::CellRole::MissedAnswer});
            result.missed++;
        }
    }

    // Encode expected eliminations for fast lookup: row * kRowStride + col * kColStride + val
    constexpr int kColStride = 9;
    constexpr int kRowStride = 81;
    auto encode = [](const core::Position& p, int v) {
        return static_cast<int>((p.row * kRowStride) + (p.col * kColStride)) + v;
    };
    std::set<int> expected_elims;
    for (const auto& elim : step.eliminations) {
        expected_elims.insert(encode(elim.position, elim.value));
    }

    // Detect over-eliminations: user removed candidates not in expected set
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            const core::Position pos{.row = row, .col = col};
            const auto& snap_notes = snapshot.getNotes(pos);
            const auto& curr_notes = current_state.getNotes(pos);
            if (snap_notes == curr_notes) {
                continue;  // Cell unchanged — no over-eliminations possible here
            }
            for (int val = 1; val <= 9; ++val) {
                if (snap_notes.contains(val) && !curr_notes.contains(val) &&
                    !expected_elims.contains(encode(pos, val))) {
                    result.feedback.push_back({.position = pos, .role = core::CellRole::WrongAnswer});
                    result.wrong++;
                }
            }
        }
    }
    return result;
}

}  // namespace

void GameViewModel::getHint(std::optional<core::Position> pos_opt) {
    // Clear any prior hint text up front. Every non-success exit in this method previously
    // left stale content visible alongside the new errorMessage; the View observes both
    // independently and has no way to know hintMessage is from the previous request.
    hintMessage.set("");

    const int hints_remaining = getHintCount();
    if (!isGameActive() || hints_remaining <= 0) {
        if (hints_remaining <= 0) {
            errorMessage.set(std::string(core::loc("Sudoku", "No hints remaining (0/10 used)")));
        }
        return;
    }

    resetCoachingState();

    // VALIDATE FIRST - Don't consume hint if validation fails
    const auto& state = gameState.get();

    if (!pos_opt.has_value()) {
        errorMessage.set(std::string(core::loc("Sudoku", "Please select a cell first")));
        return;  // Don't consume hint
    }

    const auto& pos = *pos_opt;

    if (state.isGiven(pos)) {
        errorMessage.set(std::string(core::loc("Sudoku", "Cannot reveal hint for given cells")));
        return;  // Don't consume hint
    }

    if (state.getValue(pos) != 0) {
        errorMessage.set(std::string(core::loc("Sudoku", "Cell already has a value")));
        return;  // Don't consume hint
    }

    // Get current board state and original puzzle (for Avoidable Rectangle)
    auto board = state.extractNumbers();
    auto original_puzzle = state.extractGivenNumbers();

    // Find next solving technique using solver (with givens info). 250 ms budget keeps
    // adversarial inputs (e.g. AI Escargot) from hanging the UI thread — see plan R8.
    constexpr std::chrono::milliseconds kHintBudget{250};
    auto step_result = solver_->findNextStep(board, original_puzzle, kHintBudget);

    if (!step_result.has_value()) {
        if (step_result.error() == core::SolverError::Timeout) {
            errorMessage.set(std::string(core::loc("Sudoku", "No hint available within budget")));
        } else {
            errorMessage.set(std::string(core::loc("Sudoku", "No logical technique found for this puzzle")));
        }
        return;  // Don't consume hint
    }

    const auto& step = step_result.value();

    // Decrement hint count (consume hint)
    auto record_result = stats_manager_->recordHint(current_game_session_);
    if (!record_result.has_value()) {
        spdlog::warn("Failed to record hint: {}", statisticsErrorToString(record_result.error()));
        // Continue anyway - don't block hint on stats error
    }

    // If the step reveals a placement, apply it to the board
    if (step.type == core::SolveStepType::Placement) {
        const auto& current_state = gameState.get();

        core::Move hint_move;
        hint_move.position = step.position;
        hint_move.value = step.value;
        hint_move.move_type = core::MoveType::PlaceHint;
        hint_move.timestamp = std::chrono::steady_clock::now();
        hint_move.previous_value = current_state.getValue(step.position);
        hint_move.previous_hint_revealed = current_state.isCellHintRevealed(step.position);
        hint_move.previous_notes = current_state.getNotes(step.position);

        applyMove(hint_move);
        recordMove(hint_move, false);
    }

    // Format and display hint explanation
    std::string hint_text = formatHintExplanation(step);
    hintMessage.set(hint_text);

    spdlog::info("Hint provided: {} (SE {:.1f})", core::getTechniqueName(step.technique), step.rating);
}

core::PuzzleOrigin GameViewModel::getCurrentPuzzleOrigin() const {
    return current_puzzle_origin_;
}

void GameViewModel::importPuzzleFromString(std::string_view input) {
    if (!analyzer_) {
        errorMessage.set(std::string(core::loc("Sudoku", "Import is not available (analyzer not wired)")));
        spdlog::warn("importPuzzleFromString called without an analyzer dependency wired");
        return;
    }

    spdlog::info("Import attempt: {} bytes", input.size());

    // Parse.
    auto parsed = analyzer_->parseString(input);
    if (!parsed) {
        switch (parsed.error().code) {
            case core::ParseError::InputTooLarge:
                errorMessage.set(std::string(core::loc("Sudoku", "Pasted text is too large")));
                break;
            case core::ParseError::Empty:
                errorMessage.set(std::string(core::loc("Sudoku", "Pasted text contained no Sudoku cells")));
                break;
            case core::ParseError::WrongLength:
                errorMessage.set(
                    fmt::format(fmt::runtime(core::loc("Sudoku", "Pasted puzzle has {} cells, expected 81")),
                                parsed.error().actual_length));
                break;
            case core::ParseError::InvalidCharacter:
                errorMessage.set(fmt::format(fmt::runtime(core::loc("Sudoku", "Invalid character '{}' at position {}")),
                                             parsed.error().bad_char, parsed.error().position));
                break;
        }
        spdlog::warn("Import parse failed (code={})", static_cast<int>(parsed.error().code));
        return;
    }

    // Validate.
    auto validation = analyzer_->validate(*parsed);
    if (!validation) {
        errorMessage.set(std::string(core::loc("Sudoku", "Pasted puzzle violates Sudoku rules")));
        spdlog::warn("Import validation failed ({} conflicting cells)", validation.error().conflicting_cells.size());
        return;
    }

    // Uniqueness.
    auto uniqueness = analyzer_->checkUniqueness(*parsed);
    if (uniqueness != core::UniquenessResult::Unique) {
        errorMessage.set(std::string(core::loc("Sudoku", "Pasted puzzle has multiple solutions")));
        spdlog::warn("Import uniqueness check returned {}", static_cast<int>(uniqueness));
        return;
    }

    // Build a fresh game state from the imported board.
    model::GameState new_state;
    new_state.loadPuzzle(*parsed);
    new_state.setDifficulty(core::Difficulty::Easy);

    gameState.set(new_state);
    gameState.update([](model::GameState& state) { state.startTimer(); });

    move_history_.clear();
    move_history_index_ = -1;
    last_valid_state_index_ = -1;

    // Seed rating fields to the unknown-defaults; applyDifficultyScore overwrites on success
    // and leaves them at defaults on timeout (silent fallback — see helper docs).
    current_puzzle_rating_ = 0.0;
    current_puzzle_techniques_.clear();
    current_puzzle_requires_backtracking_ = false;
    current_puzzle_origin_ = core::PuzzleOrigin::ImportedString;

    // Auto-analyze before startGameSession so the stats record uses the real rating.
    applyDifficultyScore(*parsed);

    // Start statistics session so the game is countable on completion.
    startGameSession();

    errorMessage.set("");
    hintMessage.set("");
    updateUIState();
    spdlog::info("Imported puzzle accepted");
}

void GameViewModel::exportPuzzleAsString() {
    if (!clipboard_ || !analyzer_) {
        spdlog::warn("exportPuzzleAsString called without clipboard or analyzer wired");
        return;
    }
    if (!isGameActive()) {
        return;
    }

    // Export the original givens — that's the "puzzle" the user might want to paste
    // elsewhere. The current_state (with user-entered values) isn't a puzzle anymore.
    const auto givens = gameState.get().extractGivenNumbers();
    const auto serialized = analyzer_->serializeToString(givens);
    clipboard_->setText(serialized);
    spdlog::info("Puzzle exported to clipboard ({} chars)", serialized.size());
}

void GameViewModel::enterEditMode() {
    // Finalize any in-progress game session as abandoned BEFORE we wipe the board. Without
    // this, the prior session leaks past app exit — its end_time is never written and it
    // never reaches aggregate stats. The View prompts the user about dirty state, but the
    // ViewModel must defend its statistics-manager contract independently.
    if (isGameActive()) {
        endGameSession(false);
    }

    // Clear board to an all-empty state. No move history, no givens.
    model::GameState empty_state;
    empty_state.loadPuzzle(core::BoardData{});  // all zeros
    empty_state.setDifficulty(core::Difficulty::Easy);

    gameState.set(empty_state);
    gameState.update([](model::GameState& state) { state.startTimer(); });

    move_history_.clear();
    move_history_index_ = -1;
    last_valid_state_index_ = -1;

    current_puzzle_rating_ = 0.0;
    current_puzzle_techniques_.clear();
    current_puzzle_requires_backtracking_ = false;
    current_puzzle_origin_ = core::PuzzleOrigin::Generated;  // Final origin is set at commit time.

    errorMessage.set("");
    hintMessage.set("");

    setInputMode(InputMode::EditGivens);
    updateUIState();
    spdlog::info("Entered EditGivens mode");
}

void GameViewModel::setEditModeGiven(const core::Position& pos, int value) {
    if (getInputMode() != InputMode::EditGivens) {
        return;
    }
    if (value < 0 || value > 9) {
        return;
    }

    gameState.update([&pos, value](model::GameState& state) {
        state.setValue(pos, value);
        state.setGiven(pos, value != 0);  // Givens are exactly the filled cells in edit mode.
    });
}

void GameViewModel::commitEditedPuzzle() {
    if (getInputMode() != InputMode::EditGivens) {
        return;
    }
    if (!analyzer_) {
        errorMessage.set(std::string(core::loc("Sudoku", "Commit is not available (analyzer not wired)")));
        return;
    }

    auto board = gameState.get().extractNumbers();

    // Validate.
    auto validation = analyzer_->validate(board);
    if (!validation) {
        errorMessage.set(std::string(core::loc("Sudoku", "Puzzle violates Sudoku rules")));
        spdlog::warn("Edit-mode commit: validation failed ({} conflicting cells)",
                     validation.error().conflicting_cells.size());
        return;
    }

    // Uniqueness.
    auto uniqueness = analyzer_->checkUniqueness(board);
    if (uniqueness != core::UniquenessResult::Unique) {
        errorMessage.set(std::string(core::loc("Sudoku", "Puzzle has multiple solutions")));
        spdlog::warn("Edit-mode commit: uniqueness check returned {}", static_cast<int>(uniqueness));
        return;
    }

    // Lock in the givens by reloading the board through loadPuzzle (sets the given flags).
    model::GameState locked;
    locked.loadPuzzle(board);
    locked.setDifficulty(core::Difficulty::Easy);
    gameState.set(locked);
    gameState.update([](model::GameState& state) { state.startTimer(); });

    move_history_.clear();
    move_history_index_ = -1;
    last_valid_state_index_ = -1;

    // Seed rating fields to the unknown-defaults; applyDifficultyScore overwrites on success
    // and leaves them at defaults on timeout (silent fallback — see helper docs).
    current_puzzle_rating_ = 0.0;
    current_puzzle_techniques_.clear();
    current_puzzle_requires_backtracking_ = false;
    current_puzzle_origin_ = core::PuzzleOrigin::ImportedEditMode;

    // Auto-analyze before startGameSession so the stats record uses the real rating.
    applyDifficultyScore(board);

    startGameSession();
    setInputMode(InputMode::Normal);
    errorMessage.set("");
    updateUIState();
    spdlog::info("Edit-mode commit accepted");
}

void GameViewModel::applyDifficultyScore(const core::BoardData& board) {
    if (!analyzer_) {
        return;
    }

    constexpr std::chrono::milliseconds kBudget{1000};
    const auto t0 = std::chrono::steady_clock::now();
    auto score = analyzer_->scoreDifficulty(board, kBudget);
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

    if (!score) {
        switch (score.error()) {
            case core::ScoringError::Timeout:
                // Adversarial input — leave rating at the caller's seed (unknown). Silent
                // fallback by design: the import/commit itself succeeded, surfacing an error
                // toast would conflate two outcomes.
                spdlog::info("Auto-analysis timed out after {} ms; rating left unknown", elapsed_ms);
                break;
            // Callers must validate + check uniqueness before calling. Reaching either of the
            // arms below post-validation means the strategy chain disagrees with the
            // validator/solution-counter — a real bug worth catching, but not user-facing.
            case core::ScoringError::NoSolution:
                spdlog::warn("Auto-analysis returned NoSolution post-validation — unexpected");
                break;
            case core::ScoringError::InvalidInput:
                spdlog::warn("Auto-analysis returned InvalidInput post-validation — unexpected");
                break;
        }
        return;
    }

    current_puzzle_rating_ = score->max_rating;
    current_puzzle_requires_backtracking_ = score->requires_backtracking;
    current_puzzle_techniques_.clear();
    for (int id : score->technique_ids) {
        current_puzzle_techniques_.insert(static_cast<core::SolvingTechnique>(id));
    }
    spdlog::info("Auto-analysis completed in {} ms (rating={:.1f}, techniques={})", elapsed_ms, score->max_rating,
                 score->technique_ids.size());
}

void GameViewModel::findStepByTechnique(core::SolvingTechnique technique) {
    if (!isGameActive()) {
        return;
    }

    resetCoachingState();

    const auto& state = gameState.get();
    auto board = state.extractNumbers();
    auto original_puzzle = state.extractGivenNumbers();

    auto step_result = solver_->findNextStepByTechnique(board, original_puzzle, technique);

    if (!step_result.has_value()) {
        // Both Unsolvable and InvalidBoard collapse into "this technique isn't available here"
        // for the user — the View doesn't distinguish "no such step" from "not a real strategy."
        errorMessage.set(fmt::format(fmt::runtime(core::loc("Sudoku", "No {} available in this puzzle")),
                                     core::getLocalizedTechniqueName(technique)));
        return;
    }

    const auto& step = step_result.value();
    hintMessage.set(formatHintExplanation(step));

    spdlog::info("Technique lookup served: {} (SE {:.1f})", core::getTechniqueName(step.technique), step.rating);
}

std::string GameViewModel::formatHintExplanation(const core::SolveStep& step) const {
    std::string message;

    // Technique name header
    message += std::string(core::getLocalizedTechniqueName(step.technique));
    message += ":\n\n";

    // Explanation from strategy (localized)
    message += core::getLocalizedExplanation(step);

    // Add placement suggestion if applicable
    if (step.type == core::SolveStepType::Placement) {
        message += "\n\n";
        message += core::locFormat(core::loc("Sudoku", "Suggestion: Place {0} at R{1}C{2}"), step.value,
                                   step.position.row + 1, step.position.col + 1);
    }

    return message;
}

int GameViewModel::getHintCount() const {
    if (!isGameActive() || current_game_session_ == 0) {
        return 0;
    }

    // Query StatisticsManager for current hints used
    auto stats_result = stats_manager_->getGameStats(current_game_session_);
    if (!stats_result) {
        return 0;
    }

    int max_hints = settings_manager_ ? settings_manager_->getSettings().max_hints : kDefaultMaxHints;
    return max_hints - stats_result->hints_used;
}

void GameViewModel::requestCoachingHint() {
    const int hints_remaining = getHintCount();
    if (!isGameActive() || hints_remaining <= 0) {
        if (hints_remaining <= 0) {
            errorMessage.set(std::string(core::loc("Sudoku", "No coaching hints remaining")));
        }
        return;
    }

    const auto& current_coaching = coachingState.get();

    // Already at max level — no-op
    if (current_coaching.active && current_coaching.level >= 3) {
        return;
    }

    // If not coaching yet, find and cache the next step
    if (!coaching_context_.has_value()) {
        const auto& state = gameState.get();
        auto board = state.extractNumbers();
        auto original_puzzle = state.extractGivenNumbers();

        // Same 250 ms budget as getHint — see R8.
        constexpr std::chrono::milliseconds kCoachingBudget{250};
        auto step_result = solver_->findNextStep(board, original_puzzle, kCoachingBudget);
        if (!step_result.has_value()) {
            if (step_result.error() == core::SolverError::Timeout) {
                errorMessage.set(std::string(core::loc("Sudoku", "No hint available within budget")));
            } else {
                errorMessage.set(std::string(core::loc("Sudoku", "No logical technique found")));
            }
            return;
        }
        coaching_context_ = CoachingContext{.step = step_result.value(), .snapshot = state};
    }

    // Consume one hint credit only on initial coaching request (not on level escalation)
    if (!current_coaching.active) {
        auto record_result = stats_manager_->recordHint(current_game_session_);
        if (!record_result.has_value()) {
            spdlog::warn("Failed to record coaching hint: {}", statisticsErrorToString(record_result.error()));
        }
    }

    int new_level = current_coaching.active ? current_coaching.level + 1 : 1;
    const auto& step = coaching_context_->step;

    // Get progressive hint from training infrastructure
    auto hint = core::getTrainingHint(step.technique, new_level, step);

    // For level 1, prepend technique description with what_to_look_for
    if (new_level == 1) {
        hint.text = buildLevel1Message(hint, step);
    }

    CoachingState new_state;
    new_state.active = true;
    new_state.level = new_level;
    new_state.max_level = new_level;
    new_state.message = std::move(hint.text);
    new_state.highlights = std::move(hint.highlights);
    new_state.technique = step.technique;

    // Level 3 enters TryIt phase — user can attempt the step
    if (new_level == 3) {
        new_state.phase = CoachingPhase::TryIt;
        new_state.message += "\n\n";
        new_state.message +=
            std::string(core::loc("Sudoku", "Try applying this step yourself, then press Check to verify."));
        coaching_context_->snapshot = gameState.get();
    }

    coachingState.set(std::move(new_state));

    spdlog::info("Coaching hint level {}: {} (SE {:.1f})", new_level, core::getTechniqueName(step.technique),
                 step.rating);
}

void GameViewModel::navigateCoachingLevel(int direction) {
    const auto& current = coachingState.get();
    if (!current.active) {
        return;
    }

    int target_level = current.level + direction;
    if (target_level < 1 || target_level > current.max_level) {
        return;  // Clamp — already at boundary
    }

    if (!coaching_context_.has_value()) {
        return;
    }
    const auto& step = coaching_context_->step;
    auto hint = core::getTrainingHint(step.technique, target_level, step);

    // For level 1, prepend technique description with what_to_look_for
    if (target_level == 1) {
        hint.text = buildLevel1Message(hint, step);
    }

    CoachingState new_state;
    new_state.active = true;
    new_state.level = target_level;
    new_state.max_level = current.max_level;
    new_state.message = std::move(hint.text);
    new_state.highlights = std::move(hint.highlights);
    new_state.technique = step.technique;

    coachingState.set(std::move(new_state));
}

void GameViewModel::dismissCoaching() {
    resetCoachingState();
}

bool GameViewModel::isCoachingActive() const {
    return coachingState.get().active;
}

void GameViewModel::checkCoachingAnswer() {
    const auto& current = coachingState.get();
    if (!current.active || current.phase != CoachingPhase::TryIt) {
        return;
    }
    if (!coaching_context_.has_value()) {
        return;
    }

    const auto& step = coaching_context_->step;
    const auto& snapshot = coaching_context_->snapshot;
    const auto& current_state = gameState.get();

    auto result = (step.type == core::SolveStepType::Placement)
                      ? evaluatePlacementCheck(step, snapshot, current_state)
                      : evaluateEliminationCheck(step, snapshot, current_state);

    const int total = result.correct + result.missed;
    std::string message;
    if (result.wrong > 0) {
        message = core::locFormat(core::loc("Sudoku", "Some actions were incorrect. {0}/{1} correct, {2} wrong."),
                                  result.correct, total, result.wrong);
    } else if (result.missed == 0 && result.correct > 0) {
        message = core::locFormat(core::loc("Sudoku", "Correct! You found all {0}/{1}."), result.correct, total);
    } else if (result.correct > 0) {
        message =
            core::locFormat(core::loc("Sudoku", "{0}/{1} correct, {2} missed."), result.correct, total, result.missed);
    } else {
        message = core::locFormat(core::loc("Sudoku", "0/{0} correct — try making some changes first."), total);
    }

    CoachingState new_state;
    new_state.active = true;
    new_state.phase = CoachingPhase::TryIt;
    new_state.level = current.level;
    new_state.max_level = current.max_level;
    new_state.message = std::move(message);
    new_state.highlights = std::move(result.feedback);
    new_state.technique = current.technique;
    new_state.check_submitted = true;

    coachingState.set(std::move(new_state));

    spdlog::info("Coaching check: {}/{} correct, {} missed, {} wrong", result.correct, total, result.missed,
                 result.wrong);
}

void GameViewModel::applyCoachingStep() {
    const auto& current = coachingState.get();
    if (!current.active || current.phase != CoachingPhase::TryIt) {
        return;
    }
    if (!coaching_context_.has_value()) {
        return;
    }

    const auto& step = coaching_context_->step;
    const auto& current_state = gameState.get();

    if (step.type == core::SolveStepType::Placement) {
        // Apply placement if cell doesn't already have the correct value
        if (current_state.getValue(step.position) != step.value) {
            core::Move move;
            move.position = step.position;
            move.value = step.value;
            move.move_type = core::MoveType::PlaceNumber;
            move.timestamp = std::chrono::steady_clock::now();
            move.previous_value = current_state.getValue(step.position);
            move.previous_hint_revealed = current_state.isCellHintRevealed(step.position);
            move.previous_notes = current_state.getNotes(step.position);

            applyMove(move);
            recordMove(move, false);
        }
    } else {
        // Apply eliminations — remove pencil marks that still exist
        for (const auto& elim : step.eliminations) {
            if (current_state.getNotes(elim.position).contains(elim.value)) {
                core::Move move;
                move.position = elim.position;
                move.value = elim.value;
                move.move_type = core::MoveType::RemoveNote;
                move.is_note = true;
                move.timestamp = std::chrono::steady_clock::now();
                move.previous_value = current_state.getValue(elim.position);
                move.previous_notes = current_state.getNotes(elim.position);

                applyMove(move);
                recordMove(move, false);
            }
        }
    }

    resetCoachingState();
    updateConflictHighlighting();
    updateUIState();

    spdlog::info("Coaching step applied: {}", core::getTechniqueName(step.technique));
}

void GameViewModel::resetCoachingState() {
    coaching_context_.reset();
    coachingState.set(CoachingState{});
}

void GameViewModel::resetCoachingIfNotTryIt() {
    if (coachingState.get().phase != CoachingPhase::TryIt) {
        resetCoachingState();
    }
}

std::string GameViewModel::buildLevel1Message(const core::TrainingHint& hint, const core::SolveStep& step) const {
    auto desc = core::getTechniqueDescription(step.technique);
    std::string message;
    message += std::string(core::getLocalizedTechniqueName(step.technique));
    message += "\n\n";
    message += std::string(desc.what_it_is);
    message += "\n\n";
    message += std::string(core::loc("Sudoku", "What to look for: "));
    message += std::string(desc.what_to_look_for);
    message += "\n\n";
    message += hint.text;
    return message;
}

}  // namespace sudoku::viewmodel
