// sudoku-cpp - Offline Sudoku Game
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
#include "../core/string_keys.h"
#include "core/i_game_validator.h"
#include "core/i_statistics_manager.h"
#include "core/i_sudoku_solver.h"
#include "core/observable.h"
#include "core/solve_step.h"
#include "game_view_model.h"
#include "model/game_state.h"

#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

void GameViewModel::getHint() {
    if (!isGameActive() || getHintCount() <= 0) {
        if (getHintCount() <= 0) {
            errorMessage.set(std::string(loc(core::StringKeys::HintNoRemaining)));
        }
        return;
    }

    // VALIDATE FIRST - Don't consume hint if validation fails
    const auto& state = gameState.get();
    auto pos_opt = state.getSelectedPosition();

    if (!pos_opt.has_value()) {
        errorMessage.set(std::string(loc(core::StringKeys::HintSelectCell)));
        return;  // Don't consume hint
    }

    const auto& pos = *pos_opt;

    if (state.isGiven(pos)) {
        errorMessage.set(std::string(loc(core::StringKeys::HintCannotRevealGiven)));
        return;  // Don't consume hint
    }

    if (state.getValue(pos) != 0) {
        errorMessage.set(std::string(loc(core::StringKeys::HintCellHasValue)));
        return;  // Don't consume hint
    }

    // Get current board state and original puzzle (for Avoidable Rectangle)
    auto board = state.extractNumbers();
    auto original_puzzle = state.extractGivenNumbers();

    // Find next solving technique using solver (with givens info)
    auto step_result = solver_->findNextStep(board, original_puzzle);

    if (!step_result.has_value()) {
        errorMessage.set(std::string(loc(core::StringKeys::HintNoTechnique)));
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

std::string GameViewModel::formatHintExplanation(const core::SolveStep& step) const {
    std::string message;

    // Technique name header
    message += std::string(core::getLocalizedTechniqueName(*loc_manager_, step.technique));
    message += ":\n\n";

    // Explanation from strategy (localized)
    message += core::getLocalizedExplanation(*loc_manager_, step);

    // Add placement suggestion if applicable
    if (step.type == core::SolveStepType::Placement) {
        message += "\n\n";
        message +=
            locFormat(core::StringKeys::HintSuggestionPlace, step.value, step.position.row + 1, step.position.col + 1);
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

    int max_hints = settings_manager_ ? settings_manager_->getSettings().max_hints : 10;
    return max_hints - stats_result->hints_used;
}

}  // namespace sudoku::viewmodel
