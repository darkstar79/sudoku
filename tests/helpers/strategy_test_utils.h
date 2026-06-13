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

#pragma once

#include "../../src/core/candidate_grid.h"
#include "../../src/core/i_solving_strategy.h"
#include "../../src/core/solve_step.h"
#include "../../src/core/solving_technique.h"
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
#include "../../src/core/strategies/full_house_strategy.h"
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

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sudoku::testing {

using namespace sudoku::core;

[[nodiscard]] inline std::string boardToString(const BoardData& board) {
    std::string result;
    for (size_t row = 0; row < 9; ++row) {
        auto row_span = board[row];
        result += fmt::format("{}\n", fmt::join(row_span, " "));
    }
    return result;
}

[[nodiscard]] inline std::string boardToFlatString(const BoardData& board) {
    std::string result;
    result.reserve(81);
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            result += static_cast<char>('0' + board[row][col]);
        }
    }
    return result;
}

[[nodiscard]] inline BoardData flatStringToBoard(const std::string& flat) {
    BoardData board{};
    for (size_t i = 0; i < 81 && i < flat.size(); ++i) {
        board[i / 9][i % 9] = flat[i] - '0';
    }
    return board;
}

[[nodiscard]] inline bool isBoardComplete(const BoardData& board) {
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            if (board[row][col] == 0) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] inline std::string checkPlacement(const SolveStep& step, const BoardData& working, const BoardData& truth,
                                                const BoardData& puzzle, const std::string& technique_name,
                                                int puzzle_index, int step_idx) {
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

[[nodiscard]] inline std::string checkEliminations(const SolveStep& step, const BoardData& working,
                                                   const BoardData& truth, const BoardData& puzzle,
                                                   const std::string& technique_name, int puzzle_index, int step_idx) {
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

[[nodiscard]] constexpr std::string_view getCellRoleName(CellRole role) {
    switch (role) {
        case CellRole::Pattern:
            return "Pattern";
        case CellRole::Pivot:
            return "Pivot";
        case CellRole::Wing:
            return "Wing";
        case CellRole::Fin:
            return "Fin";
        case CellRole::Roof:
            return "Roof";
        case CellRole::Floor:
            return "Floor";
        case CellRole::ChainA:
            return "ChainA";
        case CellRole::ChainB:
            return "ChainB";
        case CellRole::LinkEndpoint:
            return "LinkEndpoint";
        case CellRole::SetA:
            return "SetA";
        case CellRole::SetB:
            return "SetB";
        case CellRole::SetC:
            return "SetC";
        case CellRole::CorrectAnswer:
            return "CorrectAnswer";
        case CellRole::WrongAnswer:
            return "WrongAnswer";
        case CellRole::MissedAnswer:
            return "MissedAnswer";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view getRegionTypeName(RegionType type) {
    switch (type) {
        case RegionType::Row:
            return "Row";
        case RegionType::Col:
            return "Col";
        case RegionType::Box:
            return "Box";
        case RegionType::None:
            return "None";
    }
    return "None";
}

/// Returns the human-readable variant name for a technique's subtype, or empty if no subtype.
/// Based on the technique_subtype values set by each strategy implementation.
[[nodiscard]] inline std::string getSubtypeName(SolvingTechnique technique, int subtype) {
    if (subtype < 0) {
        // Special case: UR Type 1 doesn't set subtype (defaults to -1)
        if (technique == SolvingTechnique::UniqueRectangle) {
            return "Type 1";
        }
        return "";
    }
    switch (technique) {
        case SolvingTechnique::UniqueRectangle:
            // subtypes: 1=Type2, 2=Type3, 3=Type4, 5=Type6
            switch (subtype) {
                case 1:
                    return "Type 2";
                case 2:
                    return "Type 3";
                case 3:
                    return "Type 4";
                case 5:
                    return "Type 6";
                default:
                    return "Type " + std::to_string(subtype);
            }
        case SolvingTechnique::SimpleColoring:
            return subtype == 0 ? "Contradiction" : "Exclusion";
        case SolvingTechnique::ThreeDMedusa:
            return subtype == 0 ? "Contradiction" : "Elimination";
        case SolvingTechnique::MultiColoring:
            return subtype == 0 ? "Wrap" : "Trap";
        case SolvingTechnique::XCycles:
            switch (subtype) {
                case 0:
                    return "Rule 1";
                case 1:
                    return "Rule 2";
                case 2:
                    return "Rule 3";
                default:
                    return "Rule " + std::to_string(subtype);
            }
        case SolvingTechnique::GroupedXCycles:
            switch (subtype) {
                case 0:
                    return "Rule 1";
                case 2:
                    return "Rule 3";
                default:
                    return "Rule " + std::to_string(subtype);
            }
        default:
            return "";
    }
}

/// Returns all known variant names for a technique, or empty vector if no variants.
[[nodiscard]] inline std::vector<std::string> getKnownVariants(SolvingTechnique technique) {
    switch (technique) {
        case SolvingTechnique::UniqueRectangle:
            return {"Type 1", "Type 2", "Type 3", "Type 4", "Type 6"};
        case SolvingTechnique::SimpleColoring:
            return {"Contradiction", "Exclusion"};
        case SolvingTechnique::ThreeDMedusa:
            return {"Contradiction", "Elimination"};
        case SolvingTechnique::MultiColoring:
            return {"Wrap", "Trap"};
        case SolvingTechnique::XCycles:
            return {"Rule 1", "Rule 2", "Rule 3"};
        case SolvingTechnique::GroupedXCycles:
            return {"Rule 1", "Rule 3"};
        default:
            return {};
    }
}

/// Builds a coverage key: "Technique" or "Technique/Variant" for subtype-aware tracking.
[[nodiscard]] inline std::string coverageKey(const std::string& technique_name, SolvingTechnique technique,
                                             int subtype) {
    auto variant = getSubtypeName(technique, subtype);
    if (variant.empty()) {
        return technique_name;
    }
    return technique_name + "/" + variant;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — registering 55 strategies in difficulty order; nesting is inherent
[[nodiscard]] inline std::vector<std::unique_ptr<ISolvingStrategy>> createStrategyChain() {
    std::vector<std::unique_ptr<ISolvingStrategy>> strategies;
    // Full House runs first, mirroring SudokuSolver::initializeStrategies() — a region's last empty cell is
    // labelled FullHouse 1.0 ahead of the Naked/Hidden Single fallbacks (story 0b.4b).
    strategies.push_back(std::make_unique<FullHouseStrategy>());
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

}  // namespace sudoku::testing
