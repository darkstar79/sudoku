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

#include "../../src/core/candidate_grid.h"
#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/strategies/als_chain_strategy.h"
#include "../../src/core/strategies/als_xy_wing_strategy.h"
#include "../../src/core/strategies/als_xz_strategy.h"
#include "../../src/core/strategies/avoidable_rectangle_strategy.h"
#include "../../src/core/strategies/box_line_reduction_strategy.h"
#include "../../src/core/strategies/bug_strategy.h"
#include "../../src/core/strategies/continuous_nice_loop_strategy.h"
#include "../../src/core/strategies/death_blossom_strategy.h"
#include "../../src/core/strategies/empty_rectangle_strategy.h"
#include "../../src/core/strategies/finned_jellyfish_strategy.h"
#include "../../src/core/strategies/finned_swordfish_strategy.h"
#include "../../src/core/strategies/finned_x_wing_strategy.h"
#include "../../src/core/strategies/forcing_chain_strategy.h"
#include "../../src/core/strategies/franken_fish_strategy.h"
#include "../../src/core/strategies/grouped_nice_loop_strategy.h"
#include "../../src/core/strategies/grouped_x_cycles_strategy.h"
#include "../../src/core/strategies/hidden_pair_strategy.h"
#include "../../src/core/strategies/hidden_quad_strategy.h"
#include "../../src/core/strategies/hidden_single_strategy.h"
#include "../../src/core/strategies/hidden_triple_strategy.h"
#include "../../src/core/strategies/hidden_unique_rectangle_strategy.h"
#include "../../src/core/strategies/jellyfish_strategy.h"
#include "../../src/core/strategies/junior_exocet_strategy.h"
#include "../../src/core/strategies/kraken_fish_strategy.h"
#include "../../src/core/strategies/multi_coloring_strategy.h"
#include "../../src/core/strategies/mutant_fish_strategy.h"
#include "../../src/core/strategies/naked_pair_strategy.h"
#include "../../src/core/strategies/naked_quad_strategy.h"
#include "../../src/core/strategies/naked_single_strategy.h"
#include "../../src/core/strategies/naked_triple_strategy.h"
#include "../../src/core/strategies/nice_loop_strategy.h"
#include "../../src/core/strategies/pointing_pair_strategy.h"
#include "../../src/core/strategies/region_forcing_chain_strategy.h"
#include "../../src/core/strategies/remote_pairs_strategy.h"
#include "../../src/core/strategies/sashimi_jellyfish_strategy.h"
#include "../../src/core/strategies/sashimi_swordfish_strategy.h"
#include "../../src/core/strategies/sashimi_x_wing_strategy.h"
#include "../../src/core/strategies/simple_coloring_strategy.h"
#include "../../src/core/strategies/skyscraper_strategy.h"
#include "../../src/core/strategies/sue_de_coq_strategy.h"
#include "../../src/core/strategies/swordfish_strategy.h"
#include "../../src/core/strategies/three_d_medusa_strategy.h"
#include "../../src/core/strategies/two_string_kite_strategy.h"
#include "../../src/core/strategies/unique_loop_strategy.h"
#include "../../src/core/strategies/unique_rectangle_strategy.h"
#include "../../src/core/strategies/unit_forcing_chain_strategy.h"
#include "../../src/core/strategies/vwxyz_wing_strategy.h"
#include "../../src/core/strategies/w_wing_strategy.h"
#include "../../src/core/strategies/wxyz_wing_strategy.h"
#include "../../src/core/strategies/x_cycles_strategy.h"
#include "../../src/core/strategies/x_wing_strategy.h"
#include "../../src/core/strategies/xy_chain_strategy.h"
#include "../../src/core/strategies/xy_wing_strategy.h"
#include "../../src/core/strategies/xyz_wing_strategy.h"
#include "../../src/core/sudoku_solver.h"

#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

using namespace sudoku::core;

namespace {

std::string boardToString(const BoardData& board) {
    std::string result;
    for (size_t row = 0; row < 9; ++row) {
        auto row_span = board[row];
        result += fmt::format("{}\n", fmt::join(row_span, " "));
    }
    return result;
}

/// Replay the solve_path step-by-step and find the first wrong deduction.
/// Returns the technique name of the first wrong step, or empty string if all correct.
std::string replayAndCapture(const BoardData& puzzle, const BoardData& truth, const std::vector<SolveStep>& solve_path,
                             int puzzle_index) {
    auto working = puzzle;
    CandidateGrid candidates(working);

    for (size_t step_idx = 0; step_idx < solve_path.size(); ++step_idx) {
        const auto& solve_step = solve_path[step_idx];
        auto technique_name = std::string(getTechniqueName(solve_step.technique));

        if (solve_step.type == SolveStepType::Placement) {
            int correct_value = truth[solve_step.position.row][solve_step.position.col];
            if (solve_step.value != correct_value) {
                std::cerr << "\n=== WRONG PLACEMENT at puzzle #" << puzzle_index << ", step #" << step_idx << " ===\n"
                          << "Technique: " << technique_name << "\n"
                          << "Position: R" << (solve_step.position.row + 1) << "C" << (solve_step.position.col + 1)
                          << "\n"
                          << "Placed: " << solve_step.value << " (WRONG)\n"
                          << "Correct: " << correct_value << "\n"
                          << "Explanation: " << solve_step.explanation << "\n"
                          << "Board before step:\n"
                          << boardToString(working) << "\n"
                          << "Ground truth:\n"
                          << boardToString(truth) << "\n"
                          << "Original puzzle:\n"
                          << boardToString(puzzle) << "\n";
                return technique_name;
            }
            working[solve_step.position.row][solve_step.position.col] = solve_step.value;
            candidates.placeValue(solve_step.position.row, solve_step.position.col, solve_step.value);
        } else {
            for (const auto& elim : solve_step.eliminations) {
                int correct_value = truth[elim.position.row][elim.position.col];
                if (elim.value == correct_value) {
                    std::cerr << "\n=== WRONG ELIMINATION at puzzle #" << puzzle_index << ", step #" << step_idx
                              << " ===\n"
                              << "Technique: " << technique_name << "\n"
                              << "Eliminated: " << elim.value << " from R" << (elim.position.row + 1) << "C"
                              << (elim.position.col + 1) << "\n"
                              << "But correct value for that cell is: " << correct_value << "\n"
                              << "Explanation: " << solve_step.explanation << "\n"
                              << "Board before step:\n"
                              << boardToString(working) << "\n"
                              << "Ground truth:\n"
                              << boardToString(truth) << "\n"
                              << "Original puzzle:\n"
                              << boardToString(puzzle) << "\n";
                    return technique_name;
                }
                candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
            }
        }
    }

    return "";
}

bool isBoardComplete(const BoardData& board) {
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            if (board[row][col] == 0) {
                return false;
            }
        }
    }
    return true;
}

std::string checkPlacement(const SolveStep& step, const BoardData& working, const BoardData& truth,
                           const BoardData& puzzle, const std::string& technique_name, int puzzle_index, int step_idx) {
    int correct = truth[step.position.row][step.position.col];
    if (step.value != correct) {
        std::cerr << "\n=== WRONG PLACEMENT (step-by-step) at puzzle #" << puzzle_index << ", step #" << step_idx
                  << " ===\n"
                  << "Technique: " << technique_name << "\n"
                  << "Position: R" << (step.position.row + 1) << "C" << (step.position.col + 1) << "\n"
                  << "Placed: " << step.value << " (WRONG), Correct: " << correct << "\n"
                  << "Explanation: " << step.explanation << "\n"
                  << "Board before step:\n"
                  << boardToString(working) << "Ground truth:\n"
                  << boardToString(truth) << "Original puzzle:\n"
                  << boardToString(puzzle) << "\n";
        return technique_name;
    }
    return "";
}

std::string checkEliminations(const SolveStep& step, const BoardData& working, const BoardData& truth,
                              const BoardData& puzzle, const std::string& technique_name, int puzzle_index,
                              int step_idx) {
    for (const auto& elim : step.eliminations) {
        int correct = truth[elim.position.row][elim.position.col];
        if (elim.value == correct) {
            std::cerr << "\n=== WRONG ELIMINATION (step-by-step) at puzzle #" << puzzle_index << ", step #" << step_idx
                      << " ===\n"
                      << "Technique: " << technique_name << "\n"
                      << "Eliminated: " << elim.value << " from R" << (elim.position.row + 1) << "C"
                      << (elim.position.col + 1) << ", Correct value: " << correct << "\n"
                      << "Explanation: " << step.explanation << "\n"
                      << "Board before step:\n"
                      << boardToString(working) << "Ground truth:\n"
                      << boardToString(truth) << "Original puzzle:\n"
                      << boardToString(puzzle) << "\n";
            return technique_name;
        }
    }
    return "";
}

std::vector<std::unique_ptr<ISolvingStrategy>> createStrategyChain() {
    std::vector<std::unique_ptr<ISolvingStrategy>> strategies;
    strategies.push_back(std::make_unique<NakedSingleStrategy>());
    strategies.push_back(std::make_unique<HiddenSingleStrategy>());
    strategies.push_back(std::make_unique<NakedPairStrategy>());
    strategies.push_back(std::make_unique<NakedTripleStrategy>());
    strategies.push_back(std::make_unique<HiddenPairStrategy>());
    strategies.push_back(std::make_unique<HiddenTripleStrategy>());
    strategies.push_back(std::make_unique<PointingPairStrategy>());
    strategies.push_back(std::make_unique<BoxLineReductionStrategy>());
    strategies.push_back(std::make_unique<NakedQuadStrategy>());
    strategies.push_back(std::make_unique<HiddenQuadStrategy>());
    strategies.push_back(std::make_unique<XWingStrategy>());
    strategies.push_back(std::make_unique<XYWingStrategy>());
    strategies.push_back(std::make_unique<SwordfishStrategy>());
    strategies.push_back(std::make_unique<SkyscraperStrategy>());
    strategies.push_back(std::make_unique<TwoStringKiteStrategy>());
    strategies.push_back(std::make_unique<XYZWingStrategy>());
    strategies.push_back(std::make_unique<WWingStrategy>());
    strategies.push_back(std::make_unique<UniqueRectangleStrategy>());
    strategies.push_back(std::make_unique<UniqueLoopStrategy>());
    strategies.push_back(std::make_unique<SimpleColoringStrategy>());
    strategies.push_back(std::make_unique<FinnedXWingStrategy>());
    strategies.push_back(std::make_unique<SashimiXWingStrategy>());
    strategies.push_back(std::make_unique<RemotePairsStrategy>());
    strategies.push_back(std::make_unique<BUGStrategy>());
    strategies.push_back(std::make_unique<HiddenUniqueRectangleStrategy>());
    strategies.push_back(std::make_unique<AvoidableRectangleStrategy>());
    strategies.push_back(std::make_unique<XCyclesStrategy>());
    strategies.push_back(std::make_unique<JellyfishStrategy>());
    strategies.push_back(std::make_unique<FinnedSwordfishStrategy>());
    strategies.push_back(std::make_unique<SashimiSwordfishStrategy>());
    strategies.push_back(std::make_unique<EmptyRectangleStrategy>());
    strategies.push_back(std::make_unique<WXYZWingStrategy>());
    strategies.push_back(std::make_unique<MultiColoringStrategy>());
    strategies.push_back(std::make_unique<ThreeDMedusaStrategy>());
    strategies.push_back(std::make_unique<FinnedJellyfishStrategy>());
    strategies.push_back(std::make_unique<SashimiJellyfishStrategy>());
    strategies.push_back(std::make_unique<XYChainStrategy>());
    strategies.push_back(std::make_unique<VWXYZWingStrategy>());
    strategies.push_back(std::make_unique<FrankenFishStrategy>());
    strategies.push_back(std::make_unique<MutantFishStrategy>());
    strategies.push_back(std::make_unique<GroupedXCyclesStrategy>());
    strategies.push_back(std::make_unique<ALSxZStrategy>());
    strategies.push_back(std::make_unique<SueDeCoqStrategy>());
    strategies.push_back(std::make_unique<ALSXYWingStrategy>());
    strategies.push_back(std::make_unique<DeathBlossomStrategy>());
    strategies.push_back(std::make_unique<ALSChainStrategy>());
    strategies.push_back(std::make_unique<ForcingChainStrategy>());
    strategies.push_back(std::make_unique<UnitForcingChainStrategy>());
    strategies.push_back(std::make_unique<RegionForcingChainStrategy>());
    strategies.push_back(std::make_unique<KrakenFishStrategy>());
    strategies.push_back(std::make_unique<JuniorExocetStrategy>());
    strategies.push_back(std::make_unique<NiceLoopStrategy>());
    strategies.push_back(std::make_unique<ContinuousNiceLoopStrategy>());
    strategies.push_back(std::make_unique<GroupedNiceLoopStrategy>());
    return strategies;
}

/// Replicate the solver's internal loop with persistent CandidateGrid.
/// This catches bugs that only manifest with accumulated eliminations.
std::string persistentCandidateReplay(const BoardData& puzzle, const BoardData& truth, int puzzle_index) {
    auto strategies = createStrategyChain();
    auto working = puzzle;
    CandidateGrid candidates(working);

    constexpr int MAX_ITERATIONS = 300;
    for (int step_idx = 0; step_idx < MAX_ITERATIONS; ++step_idx) {
        if (isBoardComplete(working)) {
            break;
        }

        // Find next step using persistent CandidateGrid (same as internal solver)
        std::optional<SolveStep> found_step;
        for (const auto& strategy : strategies) {
            auto step = strategy->findStep(working, candidates);
            if (step.has_value()) {
                found_step = step.value();
                break;
            }
        }

        if (!found_step.has_value()) {
            break;  // No strategy found — would fall back to backtracking
        }

        auto technique_name = std::string(getTechniqueName(found_step->technique));

        if (found_step->type == SolveStepType::Placement) {
            auto result = checkPlacement(*found_step, working, truth, puzzle, technique_name, puzzle_index, step_idx);
            if (!result.empty()) {
                return result;
            }
            working[found_step->position.row][found_step->position.col] = found_step->value;
            candidates.placeValue(found_step->position.row, found_step->position.col, found_step->value);
        } else {
            auto result =
                checkEliminations(*found_step, working, truth, puzzle, technique_name, puzzle_index, step_idx);
            if (!result.empty()) {
                return result;
            }
            for (const auto& elim : found_step->eliminations) {
                candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
            }
        }
    }
    return "";
}

/// Runs the correctness check over a range of seeds at a given difficulty.
/// Returns {wrong_count, generated_count, unsolvable_count}.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::tuple<int, int, int> runCorrectnessCheck(Difficulty difficulty, int num_puzzles, int start_seed = 1) {
    auto validator = std::make_shared<GameValidator>();
    PuzzleGenerator generator;
    SudokuSolver solver(validator);

    int wrong_count = 0;
    int generated = 0;
    int unsolvable_count = 0;
    std::map<std::string, int> wrong_by_technique;

    for (int seed = start_seed; seed < start_seed + num_puzzles; ++seed) {
        GenerationSettings settings{
            .difficulty = difficulty,
            .seed = static_cast<uint32_t>(seed),
            .ensure_unique = true,
            .max_attempts = 500,
        };

        auto puzzle_result = generator.generatePuzzle(settings);
        if (!puzzle_result.has_value()) {
            continue;
        }

        auto& puzzle = puzzle_result.value();
        generated++;

        const auto& truth = puzzle.solution;

        auto solve_result = solver.solvePuzzle(puzzle.board);

        if (!solve_result.has_value()) {
            unsolvable_count++;
            auto bad_technique = persistentCandidateReplay(puzzle.board, truth, seed);
            if (!bad_technique.empty()) {
                wrong_by_technique[bad_technique]++;
            } else {
                std::cerr << "\n=== UNSOLVABLE (no wrong step found) puzzle #" << seed << " ===\n"
                          << "Puzzle:\n"
                          << boardToString(puzzle.board) << "\n"
                          << "Ground truth:\n"
                          << boardToString(truth) << "\n";
            }
            wrong_count++;
            continue;
        }

        if (solve_result->solution != truth) {
            if (solve_result->used_backtracking) {
                continue;
            }
            auto bad_technique = replayAndCapture(puzzle.board, truth, solve_result->solve_path, seed);
            if (!bad_technique.empty()) {
                wrong_by_technique[bad_technique]++;
                wrong_count++;
            }
        } else if (!solve_result->used_backtracking) {
            auto bad_technique = replayAndCapture(puzzle.board, truth, solve_result->solve_path, seed);
            if (!bad_technique.empty()) {
                wrong_by_technique[bad_technique]++;
                wrong_count++;
            }
        }
    }

    if (wrong_count > 0 || unsolvable_count > 0) {
        std::cerr << "\n=== STRATEGY CORRECTNESS SUMMARY ===\n"
                  << "Generated: " << generated << " puzzles\n"
                  << "Wrong deductions: " << wrong_count << "\n"
                  << "Unsolvable (strategy corruption): " << unsolvable_count << "\n"
                  << "By technique:\n";
        for (const auto& [technique, count] : wrong_by_technique) {
            std::cerr << "  " << technique << ": " << count << "\n";
        }
    }

    return {wrong_count, generated, unsolvable_count};
}

}  // namespace

TEST_CASE("Strategy correctness: no wrong deductions on Master puzzles", "[.][strategy][correctness]") {
    constexpr int NUM_PUZZLES = 2000;
    auto [wrong, generated, unsolvable] = runCorrectnessCheck(Difficulty::Master, NUM_PUZZLES);

    INFO("Generated " << generated << " Master puzzles, " << wrong << " wrong, " << unsolvable << " unsolvable");
    REQUIRE(wrong == 0);
}
