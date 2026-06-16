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

#include "sudoku_solver.h"

#include "backtracking_solver.h"
#include "core/candidate_grid.h"
#include "core/i_game_validator.h"
#include "core/i_solving_strategy.h"
#include "core/i_sudoku_solver.h"
#include "strategies/als_chain_strategy.h"
#include "strategies/als_xy_wing_strategy.h"
#include "strategies/als_xz_strategy.h"
#include "strategies/avoidable_rectangle_strategy.h"
#include "strategies/box_line_reduction_strategy.h"
#include "strategies/bug_strategy.h"
#include "strategies/continuous_nice_loop_strategy.h"
#include "strategies/death_blossom_strategy.h"
#include "strategies/empty_rectangle_strategy.h"
#include "strategies/finned_jellyfish_strategy.h"
#include "strategies/finned_swordfish_strategy.h"
#include "strategies/finned_x_wing_strategy.h"
#include "strategies/forcing_chain_strategy.h"
#include "strategies/franken_fish_strategy.h"
#include "strategies/full_house_strategy.h"
#include "strategies/grouped_nice_loop_strategy.h"
#include "strategies/grouped_x_cycles_strategy.h"
#include "strategies/hidden_pair_strategy.h"
#include "strategies/hidden_quad_strategy.h"
#include "strategies/hidden_single_strategy.h"
#include "strategies/hidden_triple_strategy.h"
#include "strategies/hidden_unique_rectangle_strategy.h"
#include "strategies/jellyfish_strategy.h"
#include "strategies/junior_exocet_strategy.h"
#include "strategies/kraken_fish_strategy.h"
#include "strategies/multi_coloring_strategy.h"
#include "strategies/mutant_fish_strategy.h"
#include "strategies/naked_pair_strategy.h"
#include "strategies/naked_quad_strategy.h"
#include "strategies/naked_single_strategy.h"
#include "strategies/naked_triple_strategy.h"
#include "strategies/nice_loop_strategy.h"
#include "strategies/pointing_pair_strategy.h"
#include "strategies/region_forcing_chain_strategy.h"
#include "strategies/remote_pairs_strategy.h"
#include "strategies/sashimi_jellyfish_strategy.h"
#include "strategies/sashimi_swordfish_strategy.h"
#include "strategies/sashimi_x_wing_strategy.h"
#include "strategies/simple_coloring_strategy.h"
#include "strategies/skyscraper_strategy.h"
#include "strategies/sue_de_coq_strategy.h"
#include "strategies/swordfish_strategy.h"
#include "strategies/three_d_medusa_strategy.h"
#include "strategies/two_string_kite_strategy.h"
#include "strategies/unique_loop_strategy.h"
#include "strategies/unique_rectangle_strategy.h"
#include "strategies/unit_forcing_chain_strategy.h"
#include "strategies/vwxyz_wing_strategy.h"
#include "strategies/w_wing_strategy.h"
#include "strategies/wxyz_wing_strategy.h"
#include "strategies/x_cycles_strategy.h"
#include "strategies/x_wing_strategy.h"
#include "strategies/xy_chain_strategy.h"
#include "strategies/xy_wing_strategy.h"
#include "strategies/xyz_wing_strategy.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <set>
#include <utility>

#include <spdlog/spdlog.h>

namespace sudoku::core {

SudokuSolver::SudokuSolver(std::shared_ptr<IGameValidator> validator, std::shared_ptr<ITimeProvider> time_provider)
    : validator_(std::move(validator)), time_provider_(std::move(time_provider)) {
    initializeStrategies();
    buildTechniqueIndex();
}

void SudokuSolver::buildTechniqueIndex() {
    technique_index_.reserve(strategies_.size());
    for (const auto& strategy : strategies_) {
        technique_index_.emplace(strategy->getTechnique(), strategy.get());
    }
}

void SudokuSolver::initializeStrategies() {
    // Add strategies in difficulty order (easiest first). Full House (SE 1.0) runs first so a
    // region's last empty cell is labelled FullHouse 1.0 rather than a Naked Single 2.3 (story 0b.4b).
    strategies_.push_back(std::make_unique<FullHouseStrategy>());
    strategies_.push_back(std::make_unique<NakedSingleStrategy>());
    strategies_.push_back(std::make_unique<HiddenSingleStrategy>());
    strategies_.push_back(std::make_unique<NakedPairStrategy>());
    strategies_.push_back(std::make_unique<NakedTripleStrategy>());
    strategies_.push_back(std::make_unique<HiddenPairStrategy>());
    strategies_.push_back(std::make_unique<HiddenTripleStrategy>());
    strategies_.push_back(std::make_unique<PointingPairStrategy>());
    strategies_.push_back(std::make_unique<BoxLineReductionStrategy>());
    strategies_.push_back(std::make_unique<NakedQuadStrategy>());
    strategies_.push_back(std::make_unique<HiddenQuadStrategy>());
    strategies_.push_back(std::make_unique<XWingStrategy>());
    strategies_.push_back(std::make_unique<XYWingStrategy>());
    strategies_.push_back(std::make_unique<SwordfishStrategy>());
    strategies_.push_back(std::make_unique<SkyscraperStrategy>());
    strategies_.push_back(std::make_unique<TwoStringKiteStrategy>());
    strategies_.push_back(std::make_unique<XYZWingStrategy>());
    strategies_.push_back(std::make_unique<WWingStrategy>());
    strategies_.push_back(std::make_unique<UniqueRectangleStrategy>());
    strategies_.push_back(std::make_unique<UniqueLoopStrategy>());
    strategies_.push_back(std::make_unique<SimpleColoringStrategy>());
    strategies_.push_back(std::make_unique<FinnedXWingStrategy>());
    strategies_.push_back(std::make_unique<SashimiXWingStrategy>());
    strategies_.push_back(std::make_unique<RemotePairsStrategy>());
    strategies_.push_back(std::make_unique<BUGStrategy>());
    strategies_.push_back(std::make_unique<HiddenUniqueRectangleStrategy>());  // NEW 350
    strategies_.push_back(std::make_unique<AvoidableRectangleStrategy>());     // NEW 350
    strategies_.push_back(std::make_unique<XCyclesStrategy>());                // NEW 350
    strategies_.push_back(std::make_unique<JellyfishStrategy>());
    strategies_.push_back(std::make_unique<FinnedSwordfishStrategy>());
    strategies_.push_back(std::make_unique<SashimiSwordfishStrategy>());
    strategies_.push_back(std::make_unique<EmptyRectangleStrategy>());
    strategies_.push_back(std::make_unique<WXYZWingStrategy>());
    strategies_.push_back(std::make_unique<MultiColoringStrategy>());
    strategies_.push_back(std::make_unique<ThreeDMedusaStrategy>());  // NEW 400
    strategies_.push_back(std::make_unique<FinnedJellyfishStrategy>());
    strategies_.push_back(std::make_unique<SashimiJellyfishStrategy>());
    strategies_.push_back(std::make_unique<XYChainStrategy>());
    strategies_.push_back(std::make_unique<VWXYZWingStrategy>());       // NEW 450
    strategies_.push_back(std::make_unique<FrankenFishStrategy>());     // NEW 450
    strategies_.push_back(std::make_unique<MutantFishStrategy>());      // NEW 450
    strategies_.push_back(std::make_unique<GroupedXCyclesStrategy>());  // NEW 450
    strategies_.push_back(std::make_unique<ALSxZStrategy>());
    strategies_.push_back(std::make_unique<SueDeCoqStrategy>());
    strategies_.push_back(std::make_unique<ALSXYWingStrategy>());     // NEW 550
    strategies_.push_back(std::make_unique<DeathBlossomStrategy>());  // NEW 550
    strategies_.push_back(std::make_unique<ALSChainStrategy>());      // NEW 550
    strategies_.push_back(std::make_unique<ForcingChainStrategy>());
    strategies_.push_back(std::make_unique<UnitForcingChainStrategy>());
    strategies_.push_back(std::make_unique<RegionForcingChainStrategy>());
    strategies_.push_back(std::make_unique<KrakenFishStrategy>());
    strategies_.push_back(std::make_unique<JuniorExocetStrategy>());
    strategies_.push_back(std::make_unique<NiceLoopStrategy>());
    strategies_.push_back(std::make_unique<ContinuousNiceLoopStrategy>());
    strategies_.push_back(std::make_unique<GroupedNiceLoopStrategy>());
}

std::expected<SolveStep, SolverError> SudokuSolver::findNextStep(const BoardData& board) const {
    // Public API: creates a fresh CandidateGrid (stateless, for hints)
    if (isComplete(board)) {
        return std::unexpected(SolverError::Unsolvable);
    }

    CandidateGrid candidates(board);
    return findNextStep(board, candidates);
}

std::expected<SolveStep, SolverError> SudokuSolver::findNextStep(const BoardData& board,
                                                                 const BoardData& original_puzzle) const {
    // Public API with givens info: creates CandidateGrid with givens mask for Avoidable Rectangle
    if (isComplete(board)) {
        return std::unexpected(SolverError::Unsolvable);
    }

    CandidateGrid candidates(board);
    candidates.setGivensFromPuzzle(original_puzzle);
    return findNextStep(board, candidates);
}

std::expected<SolveStep, SolverError> SudokuSolver::findNextStep(const BoardData& board,
                                                                 const CandidateGrid& candidates) const {
    // Internal: uses existing CandidateGrid with accumulated eliminations
    if (isComplete(board)) {
        return std::unexpected(SolverError::Unsolvable);
    }

    for (const auto& strategy : strategies_) {
        auto step = strategy->findStep(board, candidates);
        if (step.has_value()) {
            return step.value();
        }
    }

    return std::unexpected(SolverError::Unsolvable);
}

std::expected<SolverResult, SolverError> SudokuSolver::solvePuzzle(const BoardData& board) const {
    return solvePuzzleImpl(board, std::nullopt);
}

std::expected<SolverResult, SolverError> SudokuSolver::solvePuzzle(const BoardData& board,
                                                                   std::chrono::milliseconds budget) const {
    return solvePuzzleImpl(board, time_provider_->steadyNow() + budget);
}

std::expected<SolverResult, SolverError>
SudokuSolver::solvePuzzleImpl(const BoardData& board,
                              std::optional<std::chrono::steady_clock::time_point> deadline) const {
    if (isComplete(board)) {
        return SolverResult{.solution = board, .solve_path = {}, .used_backtracking = false};
    }

    auto working_board = board;
    CandidateGrid candidates(working_board);  // Persistent candidate grid across iterations
    std::vector<SolveStep> solve_path;
    bool used_backtracking = false;

    constexpr int MAX_ITERATIONS = 200;
    int iterations = 0;

    // Wall-clock check helper: true iff a deadline is set and has been reached. Granularity is
    // "between strategy iterations" (R2 plan note) — strategies run sub-ms in the common case.
    const auto deadlineReached = [this, &deadline]() {
        return deadline.has_value() && time_provider_->steadyNow() >= *deadline;
    };
    // Post-backtracking error mapping: distinguish deadline-abort from genuine unsolvability
    // so the caller can surface "tighten budget" vs. "puzzle has no solution".
    const auto onBacktrackFailed = [&]() { return deadlineReached() ? SolverError::Timeout : SolverError::Unsolvable; };

    while (!isComplete(working_board)) {
        if (deadlineReached()) {
            return std::unexpected(SolverError::Timeout);
        }

        if (++iterations > MAX_ITERATIONS) {
            spdlog::warn("Solver hit iteration limit, using backtracking fallback");
            if (!solveWithBacktracking(working_board, deadline)) {
                return std::unexpected(onBacktrackFailed());
            }
            used_backtracking = true;
            break;
        }

        auto step_result = findNextStep(working_board, candidates);

        if (step_result.has_value()) {
            auto& step = step_result.value();
            if (!applyStep(working_board, candidates, step)) {
                return std::unexpected(SolverError::Unsolvable);
            }
            solve_path.push_back(step);
        } else {
            if (!solveWithBacktracking(working_board, deadline)) {
                return std::unexpected(onBacktrackFailed());
            }
            used_backtracking = true;
            break;
        }
    }

    return SolverResult{.solution = working_board, .solve_path = solve_path, .used_backtracking = used_backtracking};
}

std::expected<SolveStep, SolverError> SudokuSolver::findNextStep(const BoardData& board,
                                                                 const BoardData& original_puzzle,
                                                                 std::chrono::milliseconds budget) const {
    if (isComplete(board)) {
        return std::unexpected(SolverError::Unsolvable);
    }

    const auto deadline = time_provider_->steadyNow() + budget;

    CandidateGrid candidates(board);
    candidates.setGivensFromPuzzle(original_puzzle);

    // Per-strategy wall-clock check. Each strategy is a black-box (R2 plan note), so we can
    // only interrupt between calls — same granularity as the solvePuzzle loop.
    for (const auto& strategy : strategies_) {
        if (time_provider_->steadyNow() >= deadline) {
            return std::unexpected(SolverError::Timeout);
        }
        auto step = strategy->findStep(board, candidates);
        if (step.has_value()) {
            return step.value();
        }
    }

    return std::unexpected(SolverError::Unsolvable);
}

std::expected<SolveStep, SolverError> SudokuSolver::findNextStepByTechnique(const BoardData& board,
                                                                            const BoardData& original_puzzle,
                                                                            SolvingTechnique technique) const {
    if (isComplete(board)) {
        return std::unexpected(SolverError::Unsolvable);
    }

    // Givens info is required for AvoidableRectangleStrategy (it distinguishes solver-placed
    // values from puzzle givens). Propagating it for every technique is harmless — the other
    // strategies simply ignore the givens mask.
    CandidateGrid candidates(board);
    candidates.setGivensFromPuzzle(original_puzzle);

    auto it = technique_index_.find(technique);
    if (it == technique_index_.end()) {
        // No strategy registered for this technique (e.g. SolvingTechnique::Backtracking sentinel).
        return std::unexpected(SolverError::InvalidBoard);
    }

    auto step = it->second->findStep(board, candidates);
    if (step.has_value()) {
        return step.value();
    }
    return std::unexpected(SolverError::Unsolvable);
}

bool SudokuSolver::applyStep(BoardData& board, const SolveStep& step) const {
    // Public API: only handles placements (for backward compatibility)
    if (step.type == SolveStepType::Placement) {
        board[step.position.row][step.position.col] = step.value;
        return true;
    }

    // Elimination without CandidateGrid: no-op (board doesn't track pencil marks)
    return true;
}

bool SudokuSolver::applyStep(BoardData& board, CandidateGrid& candidates, const SolveStep& step) {
    // Internal: updates both board and candidate grid
    if (step.type == SolveStepType::Placement) {
        board[step.position.row][step.position.col] = step.value;
        candidates.placeValue(step.position.row, step.position.col, step.value);
        return true;
    }

    // Elimination: apply each elimination to the candidate grid
    for (const auto& elim : step.eliminations) {
        candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
    }
    return true;
}

bool SudokuSolver::isComplete(const BoardData& board) {
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            if (board[row][col] == 0) {
                return false;
            }
        }
    }
    return true;
}

bool SudokuSolver::solveWithBacktracking(BoardData& board,
                                         std::optional<std::chrono::steady_clock::time_point> deadline) const {
    BacktrackingSolver solver(validator_, time_provider_);
    return solver.solve(board, ValueSelectionStrategy::MostConstrained, /*rng*/ nullptr, deadline);
}

std::vector<SolveStep> SudokuSolver::findAllApplicableSteps(const BoardData& board) const {
    if (isComplete(board)) {
        return {};
    }

    CandidateGrid candidates(board);
    std::vector<SolveStep> results;
    std::set<SolvingTechnique> seen_techniques;

    for (const auto& strategy : strategies_) {
        auto step = strategy->findStep(board, candidates);
        if (step.has_value() && !seen_techniques.contains(step->technique)) {
            seen_techniques.insert(step->technique);
            results.push_back(std::move(*step));
        }
    }

    return results;
}

}  // namespace sudoku::core
