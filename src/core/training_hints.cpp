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

#include "training_hints.h"

#include "localized_explanations.h"
#include "string_keys.h"

#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

namespace {

/// Helper to format a localized string from a key with arguments
template <typename... Args>
[[nodiscard]] std::string locHint(const ILocalizationManager& loc, std::string_view key, Args&&... args) {
    return fmt::format(fmt::runtime(loc.getString(key)), std::forward<Args>(args)...);
}

}  // namespace

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
TrainingHint getTrainingHint(const ILocalizationManager& loc, SolvingTechnique technique, int level,
                             const SolveStep& expected) {
    auto category = getTechniqueCategory(technique);
    const auto& data = expected.explanation_data;

    TrainingHint hint;

    switch (category) {
        case TechniqueCategory::Singles: {
            if (level == 1) {
                hint.text = locHint(loc, StringKeys::HintSinglesL1, localizedPosition(loc, expected.position));
            } else if (level == 2) {
                if (data.region_type != RegionType::None) {
                    hint.text = locHint(loc, StringKeys::HintSinglesL2Region,
                                        localizedRegion(loc, data.region_type, data.region_index));
                } else {
                    hint.text =
                        locHint(loc, StringKeys::HintSinglesL2NoRegion, localizedPosition(loc, expected.position));
                }
            } else {
                hint.text = locHint(loc, StringKeys::HintSinglesL3, expected.value);
            }
            hint.highlights = {{.position = expected.position, .role = CellRole::Pattern}};
            break;
        }

        case TechniqueCategory::Subsets: {
            if (level == 1) {
                if (data.region_type != RegionType::None) {
                    hint.text = locHint(loc, StringKeys::HintSubsetsL1Region,
                                        localizedRegion(loc, data.region_type, data.region_index));
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintSubsetsL1NoRegion));
                }
            } else if (level == 2) {
                if (!data.values.empty()) {
                    hint.text = locHint(loc, StringKeys::HintSubsetsL2Values, formatValueList(data.values));
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintSubsetsL2NoValues));
                }
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintSubsetsL3));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Intersections: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = locHint(loc, StringKeys::HintIntersectionsL1Value, data.values[0]);
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintIntersectionsL1NoValue));
                }
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintIntersectionsL2));
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintIntersectionsL3));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Fish: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = locHint(loc, StringKeys::HintFishL1Value, data.values[0]);
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintFishL1NoValue));
                }
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintFishL2));
                appendDataHighlights(hint, data, CellRole::Pattern);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintFishL3));
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
                hint.text = locHint(loc, StringKeys::HintWingsL1, localizedPosition(loc, pivot));
                hint.highlights = {{.position = pivot, .role = CellRole::Pivot}};
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintWingsL2));
                appendDataHighlights(hint, data, CellRole::Wing);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintWingsL3));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::SingleDigit: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = locHint(loc, StringKeys::HintSingleDigitL1Value, data.values[0]);
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintSingleDigitL1NoValue));
                }
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintSingleDigitL2));
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintSingleDigitL3));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Coloring: {
            if (level == 1) {
                if (!data.values.empty()) {
                    hint.text = locHint(loc, StringKeys::HintColoringL1Value, data.values[0]);
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintColoringL1NoValue));
                }
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintColoringL2));
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintColoringL3));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::UniqueRect: {
            if (level == 1) {
                hint.text = std::string(loc.getString(StringKeys::HintUniqueRectL1));
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintUniqueRectL2));
                appendDataHighlights(hint, data, CellRole::Roof);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintUniqueRectL3));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Chains: {
            if (level == 1) {
                if (!data.positions.empty()) {
                    hint.text = locHint(loc, StringKeys::HintChainsL1Pos, localizedPosition(loc, data.positions[0]));
                    hint.highlights = {{.position = data.positions[0], .role = CellRole::ChainA}};
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintChainsL1NoPos));
                }
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintChainsL2));
                appendDataHighlights(hint, data, CellRole::ChainA);
            } else {
                if (expected.type == SolveStepType::Placement) {
                    hint.text = locHint(loc, StringKeys::HintChainsL3Placement, expected.value,
                                        localizedPosition(loc, expected.position));
                    hint.highlights = {{.position = expected.position, .role = CellRole::Pattern}};
                } else {
                    hint.text = std::string(loc.getString(StringKeys::HintChainsL3Elimination));
                    appendEliminationHighlights(hint, expected);
                }
            }
            break;
        }

        case TechniqueCategory::SetLogic: {
            if (level == 1) {
                hint.text = std::string(loc.getString(StringKeys::HintSetLogicL1));
            } else if (level == 2) {
                hint.text = std::string(loc.getString(StringKeys::HintSetLogicL2));
                appendDataHighlights(hint, data, CellRole::SetA);
            } else {
                hint.text = std::string(loc.getString(StringKeys::HintSetLogicL3));
                appendEliminationHighlights(hint, expected);
            }
            break;
        }

        case TechniqueCategory::Special: {
            if (level == 1) {
                hint.text = std::string(loc.getString(StringKeys::HintSpecialL1));
            } else if (level == 2) {
                if (!data.positions.empty()) {
                    hint.text = locHint(loc, StringKeys::HintSpecialL2, localizedPosition(loc, data.positions[0]));
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
