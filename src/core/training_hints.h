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

#pragma once

#include "i_game_validator.h"
#include "solve_step.h"
#include "solving_technique.h"

#include <string>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Hint data returned by the per-technique hint system
struct TrainingHint {
    std::string text;                       ///< Hint message to display
    std::vector<Position> highlight_cells;  ///< Cells to highlight on the board
    std::vector<CellRole> highlight_roles;  ///< Color role per highlighted cell
};

/// Technique category for grouping hint behavior
enum class TechniqueCategory : uint8_t {
    Singles,        ///< Naked Single, Hidden Single
    Subsets,        ///< Naked/Hidden Pair/Triple/Quad
    Intersections,  ///< Pointing Pair, Box/Line Reduction
    Fish,           ///< X-Wing, Swordfish, Jellyfish, Finned variants, Franken Fish
    Wings,          ///< XY-Wing, XYZ-Wing, WXYZ-Wing, VWXYZ-Wing, W-Wing
    SingleDigit,    ///< Skyscraper, Two-String Kite, Empty Rectangle
    Coloring,       ///< Simple Coloring, Multi-Coloring, 3D Medusa
    UniqueRect,     ///< Unique Rectangle, Hidden UR, Avoidable Rectangle
    Chains,         ///< XY-Chain, Remote Pairs, X-Cycles, Grouped X-Cycles, Forcing Chain, Nice Loop
    SetLogic,       ///< ALS-XZ, ALS-XY-Wing, Sue de Coq, Death Blossom
    Special         ///< BUG
};

/// Classify a solving technique into its hint category
[[nodiscard]] constexpr TechniqueCategory getTechniqueCategory(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        case NakedSingle:
        case HiddenSingle:
            return TechniqueCategory::Singles;
        case NakedPair:
        case NakedTriple:
        case NakedQuad:
        case HiddenPair:
        case HiddenTriple:
        case HiddenQuad:
            return TechniqueCategory::Subsets;
        case PointingPair:
        case BoxLineReduction:
            return TechniqueCategory::Intersections;
        case XWing:
        case Swordfish:
        case Jellyfish:
        case FinnedXWing:
        case FinnedSwordfish:
        case FinnedJellyfish:
        case FrankenFish:
        case MutantFish:
        case SashimiXWing:
        case SashimiSwordfish:
        case SashimiJellyfish:
        case KrakenFish:
            return TechniqueCategory::Fish;
        case XYWing:
        case XYZWing:
        case WXYZWing:
        case VWXYZWing:
        case WWing:
            return TechniqueCategory::Wings;
        case Skyscraper:
        case TwoStringKite:
        case EmptyRectangle:
            return TechniqueCategory::SingleDigit;
        case SimpleColoring:
        case MultiColoring:
        case ThreeDMedusa:
            return TechniqueCategory::Coloring;
        case UniqueRectangle:
        case HiddenUniqueRectangle:
        case AvoidableRectangle:
        case UniqueLoop:
            return TechniqueCategory::UniqueRect;
        case XYChain:
        case RemotePairs:
        case XCycles:
        case GroupedXCycles:
        case ForcingChain:
        case NiceLoop:
        case ContinuousNiceLoop:
        case GroupedNiceLoop:
        case UnitForcingChain:
        case RegionForcingChain:
            return TechniqueCategory::Chains;
        case ALSxZ:
        case ALSXYWing:
        case SueDeCoq:
        case DeathBlossom:
        case ALSChain:
            return TechniqueCategory::SetLogic;
        case BUG:
        case JuniorExocet:
        case Backtracking:
            return TechniqueCategory::Special;
    }
    return TechniqueCategory::Special;
}

/// Format a position as "R{r}C{c}" (1-indexed)
[[nodiscard]] inline std::string formatPos(const Position& pos) {
    return fmt::format("R{}C{}", pos.row + 1, pos.col + 1);
}

/// Format a region type + index as human-readable text
[[nodiscard]] inline std::string formatRegion(RegionType type, size_t index) {
    switch (type) {
        case RegionType::Row:
            return fmt::format("row {}", index + 1);
        case RegionType::Col:
            return fmt::format("column {}", index + 1);
        case RegionType::Box:
            return fmt::format("box {}", index + 1);
        case RegionType::None:
            return "unknown region";
    }
    return "unknown region";
}

/// Append explanation_data positions to hint with per-position roles (or a default)
inline void appendDataHighlights(TrainingHint& hint, const ExplanationData& data, CellRole default_role) {
    for (size_t i = 0; i < data.positions.size(); ++i) {
        hint.highlight_cells.push_back(data.positions[i]);
        auto role = (i < data.position_roles.size()) ? data.position_roles[i] : default_role;
        hint.highlight_roles.push_back(role);
    }
}

/// Append elimination positions to hint (highlighted as Fin/target)
inline void appendEliminationHighlights(TrainingHint& hint, const SolveStep& expected) {
    for (const auto& elim : expected.eliminations) {
        hint.highlight_cells.push_back(elim.position);
        hint.highlight_roles.push_back(CellRole::Fin);
    }
}

/// Get a per-technique progressive hint for training exercises.
/// @param technique The technique being practiced
/// @param level Hint level (1-3)
/// @param expected The expected solving step (contains positions, values, explanation data)
/// @return TrainingHint with text and cells to highlight
// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — per-category hint dispatch with 3 levels each; inherent branching
[[nodiscard]] inline TrainingHint getTrainingHint(SolvingTechnique technique, int level, const SolveStep& expected) {
    auto category = getTechniqueCategory(technique);
    const auto& data = expected.explanation_data;

    TrainingHint hint;

    switch (category) {
        case TechniqueCategory::Singles: {
            if (level == 1) {
                hint.text = fmt::format("Look at cell {}.", formatPos(expected.position));
                hint.highlight_cells = {expected.position};
                hint.highlight_roles = {CellRole::Pattern};
            } else if (level == 2) {
                std::string region_desc;
                if (data.region_type != RegionType::None) {
                    region_desc = fmt::format("Focus on {} — count the candidates.",
                                              formatRegion(data.region_type, data.region_index));
                } else {
                    region_desc = fmt::format("Count the candidates in cell {}.", formatPos(expected.position));
                }
                hint.text = region_desc;
                hint.highlight_cells = {expected.position};
                hint.highlight_roles = {CellRole::Pattern};
            } else {
                hint.text = fmt::format("The value is {}.", expected.value);
                hint.highlight_cells = {expected.position};
                hint.highlight_roles = {CellRole::Pattern};
            }
            break;
        }

        case TechniqueCategory::Subsets: {
            if (level == 1) {
                if (data.region_type != RegionType::None) {
                    hint.text = fmt::format("Focus on {}.", formatRegion(data.region_type, data.region_index));
                } else {
                    hint.text = "Look for cells that share the same candidates in a unit.";
                }
            } else if (level == 2) {
                hint.text = "These cells form the subset:";
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text = "Eliminate candidates from cells that see all subset cells.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Intersections: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = fmt::format("Look for value {} confined to an intersection.", data.values[0]);
                } else {
                    hint.text = "Look for a candidate confined to the intersection of a box and a line.";
                }
            } else if (level == 2) {
                hint.text = "The intersection cells:";
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text = "Eliminate the candidate from cells outside the intersection.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Fish: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = fmt::format("Look for a fish pattern on value {}.", data.values[0]);
                } else {
                    hint.text = "Look for a fish pattern (rows/columns with restricted candidate positions).";
                }
            } else if (level == 2) {
                hint.text = "Base and cover sets:";
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text = "Eliminate the candidate from cover set cells outside the base set.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Wings: {
            if (level == 1) {
                // Find the pivot (first position, or first with Pivot role)
                Position pivot = expected.position;
                for (size_t i = 0; i < data.position_roles.size(); ++i) {
                    if (data.position_roles[i] == CellRole::Pivot) {
                        pivot = data.positions[i];
                        break;
                    }
                }
                hint.text = fmt::format("Find the pivot cell at {}.", formatPos(pivot));
                hint.highlight_cells = {pivot};
                hint.highlight_roles = {CellRole::Pivot};
            } else if (level == 2) {
                hint.text = "Pivot and wing cells:";
                appendDataHighlights(hint, data, CellRole::Wing);
            } else {
                hint.text = "Eliminate the shared candidate from cells that see all wing endpoints.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::SingleDigit: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = fmt::format("Look for conjugate pairs on value {}.", data.values[0]);
                } else {
                    hint.text = "Look for conjugate pairs (cells where a digit appears exactly twice in a unit).";
                }
            } else if (level == 2) {
                hint.text = "The chain cells:";
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                hint.text = "Cells that see both endpoints of the pattern can be eliminated.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Coloring: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = fmt::format("Build a coloring chain on value {}.", data.values[0]);
                } else {
                    hint.text = "Start coloring conjugate pairs with two alternating colors.";
                }
            } else if (level == 2) {
                hint.text = "The coloring chain:";
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                hint.text = "One color must be false — eliminate from cells that see both colors.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::UniqueRect: {
            if (level == 1) {
                hint.text = "Look for a deadly pattern — four cells forming a rectangle across two boxes.";
            } else if (level == 2) {
                hint.text = "The rectangle corners:";
                appendDataHighlights(hint, data, CellRole::Roof);
            } else {
                hint.text = "To avoid the deadly pattern, eliminate the candidate that would complete it.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Chains: {
            if (level == 1) {
                if (!data.positions.empty()) {
                    hint.text = fmt::format("Start the chain from cell {}.", formatPos(data.positions[0]));
                    hint.highlight_cells = {data.positions[0]};
                    hint.highlight_roles = {CellRole::ChainA};
                } else {
                    hint.text = "Look for a chain of linked cells with alternating strong/weak links.";
                }
            } else if (level == 2) {
                hint.text = "The chain path:";
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                if (expected.type == SolveStepType::Placement) {
                    hint.text =
                        fmt::format("All chains lead to value {} at {}.", expected.value, formatPos(expected.position));
                    hint.highlight_cells = {expected.position};
                    hint.highlight_roles = {CellRole::Pattern};
                } else {
                    hint.text = "Eliminate candidates that contradict the chain logic.";
                    appendEliminationHighlights(hint, expected);
                }
            }
            break;
        }

        case TechniqueCategory::SetLogic: {
            if (level == 1) {
                hint.text = "Look for an Almost Locked Set (a group of N cells with N+1 candidates).";
            } else if (level == 2) {
                hint.text = "The ALS cells and restricted common:";
                appendDataHighlights(hint, data, CellRole::SetA);
            } else {
                hint.text = "Eliminate candidates from cells that see all relevant ALS members.";
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Special: {
            if (level == 1) {
                hint.text = "Look for the cell with three candidates (the only non-bivalue cell).";
            } else if (level == 2) {
                if (!data.positions.empty()) {
                    hint.text = fmt::format("The key cell is {}.", formatPos(data.positions[0]));
                    hint.highlight_cells = {data.positions[0]};
                    hint.highlight_roles = {CellRole::Pattern};
                } else {
                    hint.text = expected.explanation;
                }
            } else {
                hint.text = expected.explanation;
                appendEliminationHighlights(hint, expected);
            }
            break;
        }
    }

    return hint;
}

}  // namespace sudoku::core
