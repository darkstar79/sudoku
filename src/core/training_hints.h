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

namespace sudoku::core {

/// Hint data returned by the per-technique hint system
struct TrainingHint {
    std::string text;                       ///< Hint message to display
    std::vector<CellHighlight> highlights;  ///< Cells to highlight on the board with their roles
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

/// Append explanation_data positions to hint with per-position roles (or a default)
void appendDataHighlights(TrainingHint& hint, const ExplanationData& data, CellRole default_role);

/// Append elimination positions to hint (highlighted as Fin/target)
void appendEliminationHighlights(TrainingHint& hint, const SolveStep& expected);

/// Get a per-technique progressive hint for training exercises.
/// @param technique The technique being practiced
/// @param level Hint level (1-3)
/// @param expected The expected solving step (contains positions, values, explanation data)
/// @return TrainingHint with text and cells to highlight
[[nodiscard]] TrainingHint getTrainingHint(SolvingTechnique technique, int level, const SolveStep& expected);

}  // namespace sudoku::core
