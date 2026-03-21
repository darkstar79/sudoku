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

#include "training_answer_validator.h"

#include "core/constants.h"
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

#include <bit>

namespace sudoku::core {

namespace {

/// Maximum iterations when searching for all steps (prevents infinite loops)
inline constexpr int kMaxFindAllIterations = 100;

/// Convert a SolveStep's eliminations to the set form used for comparison
[[nodiscard]] std::set<std::tuple<size_t, size_t, int>> eliminationsToSet(const SolveStep& step) {
    std::set<std::tuple<size_t, size_t, int>> result;
    for (const auto& elim : step.eliminations) {
        result.emplace(elim.position.row, elim.position.col, elim.value);
    }
    return result;
}

/// Find a matching placement step from all strategy-generated steps
[[nodiscard]] std::optional<SolveStep> findMatchingPlacement(const BoardData& board, const CandidateGrid& candidates,
                                                             SolvingTechnique technique, Position position, int value,
                                                             SolveStepType required_type = SolveStepType::Placement) {
    auto all_steps = TrainingAnswerValidator::findAllSteps(board, candidates, technique);
    for (auto& step : all_steps) {
        if (step.type == required_type && step.position == position && step.value == value) {
            return std::move(step);
        }
    }
    return std::nullopt;
}

/// Check if value appears as candidate in exactly one empty cell in a unit
[[nodiscard]] int countCandidateInRow(const BoardData& board, const CandidateGrid& candidates, size_t row, int value) {
    int count = 0;
    for (size_t c = 0; c < BOARD_SIZE; ++c) {
        if (board[row][c] == 0 && candidates.isAllowed(row, c, value)) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] int countCandidateInCol(const BoardData& board, const CandidateGrid& candidates, size_t col, int value) {
    int count = 0;
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        if (board[r][col] == 0 && candidates.isAllowed(r, col, value)) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] int countCandidateInBox(const BoardData& board, const CandidateGrid& candidates, size_t row, size_t col,
                                      int value) {
    size_t box_row = (row / BOX_SIZE) * BOX_SIZE;
    size_t box_col = (col / BOX_SIZE) * BOX_SIZE;
    int count = 0;
    for (size_t br = 0; br < BOX_SIZE; ++br) {
        for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
            if (board[box_row + br][box_col + bc] == 0 && candidates.isAllowed(box_row + br, box_col + bc, value)) {
                ++count;
            }
        }
    }
    return count;
}

/// Check if value is a hidden single at the given position (unique candidate in any unit)
[[nodiscard]] bool isHiddenSingle(const BoardData& board, const CandidateGrid& candidates, Position position,
                                  int value) {
    return countCandidateInRow(board, candidates, position.row, value) == 1 ||
           countCandidateInCol(board, candidates, position.col, value) == 1 ||
           countCandidateInBox(board, candidates, position.row, position.col, value) == 1;
}

}  // namespace

std::optional<SolveStep> TrainingAnswerValidator::validatePlacement(const BoardData& board,
                                                                    const CandidateGrid& candidates,
                                                                    SolvingTechnique technique, Position position,
                                                                    int value) {
    if (position.row >= BOARD_SIZE || position.col >= BOARD_SIZE) {
        return std::nullopt;
    }

    // Cell must be empty and value must be a candidate
    if (board[position.row][position.col] != 0 || !candidates.isAllowed(position.row, position.col, value)) {
        return std::nullopt;
    }

    // For NakedSingle: cell must have exactly one candidate
    if (technique == SolvingTechnique::NakedSingle) {
        if (candidates.countPossibleValues(position.row, position.col) != 1) {
            return std::nullopt;
        }
        return findMatchingPlacement(board, candidates, technique, position, value);
    }

    // For HiddenSingle: value must appear as candidate in exactly one cell in a row, col, or box
    if (technique == SolvingTechnique::HiddenSingle) {
        if (!isHiddenSingle(board, candidates, position, value)) {
            return std::nullopt;
        }
        return findMatchingPlacement(board, candidates, technique, position, value);
    }

    // For other placement techniques (e.g., ForcingChain placement): use findAllSteps
    return findMatchingPlacement(board, candidates, technique, position, value);
}

std::optional<SolveStep>
TrainingAnswerValidator::validateElimination(const BoardData& board, const CandidateGrid& candidates,
                                             SolvingTechnique technique,
                                             const std::set<std::tuple<size_t, size_t, int>>& player_eliminations) {
    auto all_steps = findAllSteps(board, candidates, technique);
    for (auto& step : all_steps) {
        if (eliminationsToSet(step) == player_eliminations) {
            return std::move(step);
        }
    }
    return std::nullopt;
}

std::vector<SolveStep> TrainingAnswerValidator::findAllSteps(const BoardData& board, const CandidateGrid& candidates,
                                                             SolvingTechnique technique) {
    auto strategy = createStrategy(technique);
    if (!strategy) {
        return {};
    }

    std::vector<SolveStep> results;
    CandidateGrid working_candidates = candidates;
    auto working_board = board;

    for (int iteration = 0; iteration < kMaxFindAllIterations; ++iteration) {
        auto step = strategy->findStep(working_board, working_candidates);
        if (!step.has_value()) {
            break;
        }

        results.push_back(step.value());

        // Apply the step to find the next one
        if (step->type == SolveStepType::Placement) {
            working_board[step->position.row][step->position.col] = step->value;
            working_candidates.placeValue(step->position.row, step->position.col, step->value);
        } else {
            for (const auto& elim : step->eliminations) {
                working_candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
            }
        }
    }

    return results;
}

CandidateGrid TrainingAnswerValidator::reconstructCandidates(const BoardData& board,
                                                             const std::vector<uint16_t>& candidate_masks) {
    CandidateGrid candidates(board);

    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            if (board[r][c] != 0) {
                continue;  // Filled cells have no candidates
            }

            uint16_t stored_mask = candidate_masks[(r * BOARD_SIZE) + c];
            uint16_t current_mask = candidates.getPossibleValuesMask(r, c);

            // Eliminate candidates that are in the constraint-propagated mask but not in the stored mask
            uint16_t to_eliminate = current_mask & ~stored_mask;
            for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                if ((to_eliminate & (1 << v)) != 0) {
                    candidates.eliminateCandidate(r, c, v);
                }
            }
        }
    }

    return candidates;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — exhaustive switch over all techniques
std::unique_ptr<ISolvingStrategy> TrainingAnswerValidator::createStrategy(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        case NakedSingle:
            return std::make_unique<NakedSingleStrategy>();
        case HiddenSingle:
            return std::make_unique<HiddenSingleStrategy>();
        case NakedPair:
            return std::make_unique<NakedPairStrategy>();
        case NakedTriple:
            return std::make_unique<NakedTripleStrategy>();
        case HiddenPair:
            return std::make_unique<HiddenPairStrategy>();
        case HiddenTriple:
            return std::make_unique<HiddenTripleStrategy>();
        case PointingPair:
            return std::make_unique<PointingPairStrategy>();
        case BoxLineReduction:
            return std::make_unique<BoxLineReductionStrategy>();
        case NakedQuad:
            return std::make_unique<NakedQuadStrategy>();
        case HiddenQuad:
            return std::make_unique<HiddenQuadStrategy>();
        case XWing:
            return std::make_unique<XWingStrategy>();
        case XYWing:
            return std::make_unique<XYWingStrategy>();
        case Swordfish:
            return std::make_unique<SwordfishStrategy>();
        case Skyscraper:
            return std::make_unique<SkyscraperStrategy>();
        case TwoStringKite:
            return std::make_unique<TwoStringKiteStrategy>();
        case XYZWing:
            return std::make_unique<XYZWingStrategy>();
        case UniqueRectangle:
            return std::make_unique<UniqueRectangleStrategy>();
        case WWing:
            return std::make_unique<WWingStrategy>();
        case SimpleColoring:
            return std::make_unique<SimpleColoringStrategy>();
        case FinnedXWing:
            return std::make_unique<FinnedXWingStrategy>();
        case RemotePairs:
            return std::make_unique<RemotePairsStrategy>();
        case BUG:
            return std::make_unique<BUGStrategy>();
        case Jellyfish:
            return std::make_unique<JellyfishStrategy>();
        case FinnedSwordfish:
            return std::make_unique<FinnedSwordfishStrategy>();
        case EmptyRectangle:
            return std::make_unique<EmptyRectangleStrategy>();
        case WXYZWing:
            return std::make_unique<WXYZWingStrategy>();
        case FinnedJellyfish:
            return std::make_unique<FinnedJellyfishStrategy>();
        case XYChain:
            return std::make_unique<XYChainStrategy>();
        case MultiColoring:
            return std::make_unique<MultiColoringStrategy>();
        case ALSxZ:
            return std::make_unique<ALSxZStrategy>();
        case SueDeCoq:
            return std::make_unique<SueDeCoqStrategy>();
        case ForcingChain:
            return std::make_unique<ForcingChainStrategy>();
        case NiceLoop:
            return std::make_unique<NiceLoopStrategy>();
        case XCycles:
            return std::make_unique<XCyclesStrategy>();
        case ThreeDMedusa:
            return std::make_unique<ThreeDMedusaStrategy>();
        case HiddenUniqueRectangle:
            return std::make_unique<HiddenUniqueRectangleStrategy>();
        case AvoidableRectangle:
            return std::make_unique<AvoidableRectangleStrategy>();
        case ALSXYWing:
            return std::make_unique<ALSXYWingStrategy>();
        case DeathBlossom:
            return std::make_unique<DeathBlossomStrategy>();
        case VWXYZWing:
            return std::make_unique<VWXYZWingStrategy>();
        case FrankenFish:
            return std::make_unique<FrankenFishStrategy>();
        case MutantFish:
            return std::make_unique<MutantFishStrategy>();
        case GroupedXCycles:
            return std::make_unique<GroupedXCyclesStrategy>();
        case SashimiXWing:
            return std::make_unique<SashimiXWingStrategy>();
        case SashimiSwordfish:
            return std::make_unique<SashimiSwordfishStrategy>();
        case SashimiJellyfish:
            return std::make_unique<SashimiJellyfishStrategy>();
        case UnitForcingChain:
            return std::make_unique<UnitForcingChainStrategy>();
        case RegionForcingChain:
            return std::make_unique<RegionForcingChainStrategy>();
        case KrakenFish:
            return std::make_unique<KrakenFishStrategy>();
        case ALSChain:
            return std::make_unique<ALSChainStrategy>();
        case JuniorExocet:
            return std::make_unique<JuniorExocetStrategy>();
        case UniqueLoop:
            return std::make_unique<UniqueLoopStrategy>();
        case GroupedNiceLoop:
            return std::make_unique<GroupedNiceLoopStrategy>();
        case ContinuousNiceLoop:
            return std::make_unique<ContinuousNiceLoopStrategy>();
        case Backtracking:
            return nullptr;
    }
    return nullptr;
}

}  // namespace sudoku::core
