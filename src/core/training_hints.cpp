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

#include "training_hints.h"

#include "core/i18n_helpers.h"
#include "localized_explanations.h"

#include <string>
#include <vector>

namespace sudoku::core {

void appendDataHighlights(TrainingHint& hint, const ExplanationData& data, CellRole default_role) {
    for (size_t i = 0; i < data.positions.size(); ++i) {
        auto role = (i < data.position_roles.size()) ? data.position_roles[i] : default_role;
        hint.highlights.push_back({data.positions[i], role});
    }
}

void appendEliminationHighlights(TrainingHint& hint, const SolveStep& expected) {
    for (const auto& elim : expected.eliminations) {
        hint.highlights.push_back({elim.position, CellRole::Fin});
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — per-category hint dispatch with 3 levels each; inherent branching
TrainingHint getTrainingHint(SolvingTechnique technique, int level, const SolveStep& expected) {
    auto category = getTechniqueCategory(technique);
    const auto& data = expected.explanation_data;

    TrainingHint hint;

    switch (category) {
        case TechniqueCategory::Singles: {
            if (level == 1) {
                hint.text =
                    core::locFormat(core::loc("Sudoku", "Look at cell {0}."), localizedPosition(expected.position));
            } else if (level == 2) {
                if (data.region_type != RegionType::None) {
                    hint.text = core::locFormat(core::loc("Sudoku", "Focus on {0} — count the candidates."),
                                                localizedRegion(data.region_type, data.region_index));
                } else {
                    hint.text = core::locFormat(core::loc("Sudoku", "Count the candidates in cell {0}."),
                                                localizedPosition(expected.position));
                }
            } else {
                hint.text = core::locFormat(core::loc("Sudoku", "The value is {0}."), expected.value);
            }
            hint.highlights = {{.position = expected.position, .role = CellRole::Pattern}};
            break;
        }

        case TechniqueCategory::Subsets: {
            if (level == 1) {
                if (data.region_type != RegionType::None) {
                    hint.text = core::locFormat(core::loc("Sudoku", "Focus on {0}."),
                                                localizedRegion(data.region_type, data.region_index));
                } else {
                    hint.text =
                        std::string(core::loc("Sudoku", "Look for cells that share the same candidates in a unit."));
                }
            } else if (level == 2) {
                if (!data.values.empty()) {
                    hint.text = core::locFormat(
                        core::loc("Sudoku", "These cells form a [{0}] subset. Values in the subset can only go in "
                                            "these cells — eliminate them from other cells in the region."),
                        formatValueList(data.values));
                } else {
                    hint.text = std::string(
                        core::loc("Sudoku", "These cells form the subset. Values in the subset can only go in these "
                                            "cells — eliminate them from other cells in the region."));
                }
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text =
                    std::string(core::loc("Sudoku", "Eliminate candidates from cells that see all subset cells."));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Intersections: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = core::locFormat(core::loc("Sudoku", "Look for value {0} confined to an intersection."),
                                                data.values[0]);
                } else {
                    hint.text = std::string(
                        core::loc("Sudoku", "Look for a candidate confined to the intersection of a box and a line."));
                }
            } else if (level == 2) {
                hint.text = std::string(core::loc(
                    "Sudoku", "The intersection cells. The candidate is confined to these cells — eliminate it from "
                              "other cells in the line or box outside this intersection."));
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text =
                    std::string(core::loc("Sudoku", "Eliminate the candidate from cells outside the intersection."));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Fish: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text =
                        core::locFormat(core::loc("Sudoku", "Look for a fish pattern on value {0}."), data.values[0]);
                } else {
                    hint.text = std::string(core::loc(
                        "Sudoku", "Look for a fish pattern (rows/columns with restricted candidate positions)."));
                }
            } else if (level == 2) {
                hint.text = std::string(
                    core::loc("Sudoku", "Base and cover sets. Blue cells are the base set (rows/columns where the "
                                        "candidate is restricted). Green cells are the cover set. Eliminate the "
                                        "candidate from cover set cells that aren't in the base set."));
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text = std::string(
                    core::loc("Sudoku", "Eliminate the candidate from cover set cells outside the base set."));
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
                hint.text =
                    core::locFormat(core::loc("Sudoku", "Find the pivot cell at {0}."), localizedPosition(pivot));
                hint.highlights = {{.position = pivot, .role = CellRole::Pivot}};
            } else if (level == 2) {
                hint.text = std::string(core::loc(
                    "Sudoku", "Pivot and wing cells. The orange pivot connects to the green wings. Candidates shared "
                              "by both wings can be eliminated from cells that see all wing endpoints."));
                appendDataHighlights(hint, data, CellRole::Wing);
            } else {
                hint.text = std::string(
                    core::loc("Sudoku", "Eliminate the shared candidate from cells that see all wing endpoints."));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::SingleDigit: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text =
                        core::locFormat(core::loc("Sudoku", "Look for conjugate pairs on value {0}."), data.values[0]);
                } else {
                    hint.text = std::string(core::loc(
                        "Sudoku", "Look for conjugate pairs (cells where a digit appears exactly twice in a unit)."));
                }
            } else if (level == 2) {
                hint.text = std::string(
                    core::loc("Sudoku", "The chain cells. These cells form conjugate pairs (a digit appears exactly "
                                        "twice in a unit). Follow the alternating pattern to find eliminations."));
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                hint.text =
                    std::string(core::loc("Sudoku", "Cells that see both endpoints of the pattern can be eliminated."));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Coloring: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text =
                        core::locFormat(core::loc("Sudoku", "Build a coloring chain on value {0}."), data.values[0]);
                } else {
                    hint.text =
                        std::string(core::loc("Sudoku", "Start coloring conjugate pairs with two alternating colors."));
                }
            } else if (level == 2) {
                hint.text = std::string(core::loc(
                    "Sudoku", "The coloring chain. Blue and green are two alternating colors — one must be true, one "
                              "false. Cells that see both colors can have the candidate eliminated."));
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                hint.text = std::string(
                    core::loc("Sudoku", "One color must be false — eliminate from cells that see both colors."));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::UniqueRect: {
            if (level == 1) {
                hint.text = std::string(core::loc(
                    "Sudoku", "Look for a deadly pattern — four cells forming a rectangle across two boxes."));
            } else if (level == 2) {
                hint.text = std::string(core::loc(
                    "Sudoku",
                    "The rectangle corners. These four cells across two boxes form a potential deadly pattern. To keep "
                    "the puzzle unique, eliminate the candidate that would complete the rectangle."));
                appendDataHighlights(hint, data, CellRole::Roof);
            } else {
                hint.text = std::string(core::loc(
                    "Sudoku", "To avoid the deadly pattern, eliminate the candidate that would complete it."));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Chains: {
            if (level == 1) {
                if (!data.positions.empty()) {
                    hint.text = core::locFormat(core::loc("Sudoku", "Start the chain from cell {0}."),
                                                localizedPosition(data.positions[0]));
                    hint.highlights = {{.position = data.positions[0], .role = CellRole::ChainA}};
                } else {
                    hint.text = std::string(
                        core::loc("Sudoku", "Look for a chain of linked cells with alternating strong/weak links."));
                }
            } else if (level == 2) {
                hint.text = std::string(
                    core::loc("Sudoku", "The chain path. Follow the alternating strong (blue) and weak (green) "
                                        "links. The chain's logic forces a conclusion at the endpoints."));
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                if (expected.type == SolveStepType::Placement) {
                    hint.text = core::locFormat(core::loc("Sudoku", "All chains lead to value {0} at {1}."),
                                                expected.value, localizedPosition(expected.position));
                    hint.highlights = {{.position = expected.position, .role = CellRole::Pattern}};
                } else {
                    hint.text =
                        std::string(core::loc("Sudoku", "Eliminate candidates that contradict the chain logic."));
                    appendEliminationHighlights(hint, expected);
                }
            }
            break;
        }

        case TechniqueCategory::SetLogic: {
            if (level == 1) {
                hint.text = std::string(
                    core::loc("Sudoku", "Look for an Almost Locked Set (a group of N cells with N+1 candidates)."));
            } else if (level == 2) {
                hint.text = std::string(core::loc(
                    "Sudoku",
                    "The ALS cells and restricted common. An ALS is N cells with N+1 candidates. The restricted common "
                    "candidate links the sets — eliminations apply to cells that see all relevant ALS members."));
                appendDataHighlights(hint, data, CellRole::SetA);
            } else {
                hint.text = std::string(
                    core::loc("Sudoku", "Eliminate candidates from cells that see all relevant ALS members."));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Special: {
            if (level == 1) {
                hint.text = std::string(
                    core::loc("Sudoku", "Look for the cell with three candidates (the only non-bivalue cell)."));
            } else if (level == 2) {
                if (!data.positions.empty()) {
                    hint.text = core::locFormat(core::loc("Sudoku", "The key cell is {0}."),
                                                localizedPosition(data.positions[0]));
                    hint.highlights = {{.position = data.positions[0], .role = CellRole::Pattern}};
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
