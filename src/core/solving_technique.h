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

#include "i_localization_manager.h"
#include "string_keys.h"

#include <cstdint>
#include <string_view>

namespace sudoku::core {

/// Sudoku solving techniques ordered by difficulty.
/// Ratings based on the Sudoku Explainer (SE) scale by Nicolas Juillerat.
enum class SolvingTechnique : uint8_t {
    NakedSingle = 0,             ///< Only one candidate in cell (SE 2.3)
    HiddenSingle = 1,            ///< Value can only appear in one cell in region (SE 1.5)
    NakedPair = 2,               ///< Two cells with same two candidates (SE 3.0)
    NakedTriple = 3,             ///< Three cells with same three candidates (SE 3.6)
    HiddenPair = 4,              ///< Two values confined to two cells in region (SE 3.4)
    HiddenTriple = 5,            ///< Three values confined to three cells in region (SE 4.0)
    PointingPair = 6,            ///< Candidate in box confined to one row/col → eliminate from rest of row/col (SE 2.6)
    BoxLineReduction = 7,        ///< Candidate in row/col confined to one box → eliminate from rest of box (SE 2.8)
    NakedQuad = 8,               ///< Four cells with same four candidates (SE 5.0)
    HiddenQuad = 9,              ///< Four values confined to four cells in region (SE 5.4)
    XWing = 10,                  ///< Candidate in 2 rows appears in same 2 cols → eliminate from cols (SE 3.2)
    XYWing = 11,                 ///< Pivot+wings pattern eliminates shared candidate (SE 4.2)
    Swordfish = 12,              ///< Candidate in 3 rows appears in same 3 cols → eliminate from cols (SE 3.8)
    Skyscraper = 13,             ///< Two conjugate pairs sharing one endpoint (SE 4.0)
    TwoStringKite = 14,          ///< Row+column conjugate pairs connected through box (SE 4.1)
    XYZWing = 15,                ///< Pivot(3 cands)+wings eliminate shared candidate (SE 4.4)
    UniqueRectangle = 16,        ///< Avoid deadly pattern in rectangle across 2 boxes (SE 4.5)
    WWing = 17,                  ///< Two same bivalue cells connected by strong link (SE 4.4)
    SimpleColoring = 18,         ///< Single-digit conjugate chain coloring (SE 4.0)
    FinnedXWing = 19,            ///< X-Wing with extra candidate cell (fin) (SE 3.4)
    RemotePairs = 20,            ///< Chain of bivalue cells with same pair (SE 4.5)
    BUG = 21,                    ///< Bivalue Universal Grave — single trivalue cell placement (SE 5.6)
    Jellyfish = 22,              ///< 4x4 fish pattern (SE 5.2)
    FinnedSwordfish = 23,        ///< Swordfish with extra candidate cell (fin) (SE 4.0)
    EmptyRectangle = 24,         ///< Single-digit chain through box intersection (SE 4.5)
    WXYZWing = 25,               ///< 4-cell wing pattern eliminating shared candidate (SE 4.6)
    FinnedJellyfish = 26,        ///< 4x4 fish with extra fin candidate (SE 5.4)
    XYChain = 27,                ///< Chain of bivalue cells with alternating shared values (SE 6.6)
    MultiColoring = 28,          ///< Cross-cluster single-digit coloring (SE 4.2)
    ALSxZ = 29,                  ///< Almost Locked Set XZ-rule (SE 7.5)
    SueDeCoq = 30,               ///< Two-Sector Disjoint Subset (SE 7.5)
    ForcingChain = 31,           ///< Cell forcing chains — assume each candidate, propagate (SE 7.5)
    NiceLoop = 32,               ///< Alternating Inference Chains (SE 7.5)
    XCycles = 33,                ///< Single-digit alternating chain cycles (SE 6.6)
    ThreeDMedusa = 34,           ///< Multi-digit coloring on (cell,digit) pairs (SE 4.4)
    HiddenUniqueRectangle = 35,  ///< Hidden deadly pattern in rectangle (SE 4.8)
    AvoidableRectangle = 36,     ///< Deadly pattern using given/solved distinction (SE 4.5)
    ALSXYWing = 37,              ///< Three ALSs linked by two restricted commons (SE 7.8)
    DeathBlossom = 38,           ///< Stem cell + petal ALSs with common elimination (SE 8.2)
    VWXYZWing = 39,              ///< 5-cell wing pattern with 5 candidate values (SE 4.8)
    FrankenFish = 40,            ///< Fish patterns with mixed row/col/box base+cover sets (SE 4.2)
    GroupedXCycles = 41,         ///< X-Cycles with grouped nodes (pointing pairs) (SE 6.8)
    Backtracking = 255           ///< Not a logical technique - fallback solver (SE 12.0)
};

/// Returns human-readable name for technique
/// @param technique The solving technique
/// @return Technique name (e.g., "Naked Single")
[[nodiscard]] constexpr std::string_view getTechniqueName(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        case NakedSingle:
            return "Naked Single";
        case HiddenSingle:
            return "Hidden Single";
        case NakedPair:
            return "Naked Pair";
        case NakedTriple:
            return "Naked Triple";
        case HiddenPair:
            return "Hidden Pair";
        case HiddenTriple:
            return "Hidden Triple";
        case PointingPair:
            return "Pointing Pair";
        case BoxLineReduction:
            return "Box/Line Reduction";
        case NakedQuad:
            return "Naked Quad";
        case HiddenQuad:
            return "Hidden Quad";
        case XWing:
            return "X-Wing";
        case XYWing:
            return "XY-Wing";
        case Swordfish:
            return "Swordfish";
        case Skyscraper:
            return "Skyscraper";
        case TwoStringKite:
            return "2-String Kite";
        case XYZWing:
            return "XYZ-Wing";
        case UniqueRectangle:
            return "Unique Rectangle";
        case WWing:
            return "W-Wing";
        case SimpleColoring:
            return "Simple Coloring";
        case FinnedXWing:
            return "Finned X-Wing";
        case RemotePairs:
            return "Remote Pairs";
        case BUG:
            return "BUG";
        case Jellyfish:
            return "Jellyfish";
        case FinnedSwordfish:
            return "Finned Swordfish";
        case EmptyRectangle:
            return "Empty Rectangle";
        case WXYZWing:
            return "WXYZ-Wing";
        case FinnedJellyfish:
            return "Finned Jellyfish";
        case XYChain:
            return "XY-Chain";
        case MultiColoring:
            return "Multi-Coloring";
        case ALSxZ:
            return "ALS-XZ";
        case SueDeCoq:
            return "Sue de Coq";
        case ForcingChain:
            return "Forcing Chain";
        case NiceLoop:
            return "Nice Loop";
        case XCycles:
            return "X-Cycles";
        case ThreeDMedusa:
            return "3D Medusa";
        case HiddenUniqueRectangle:
            return "Hidden Unique Rectangle";
        case AvoidableRectangle:
            return "Avoidable Rectangle";
        case ALSXYWing:
            return "ALS-XY-Wing";
        case DeathBlossom:
            return "Death Blossom";
        case VWXYZWing:
            return "VWXYZ-Wing";
        case FrankenFish:
            return "Franken Fish";
        case GroupedXCycles:
            return "Grouped X-Cycles";
        case Backtracking:
            return "Backtracking";
    }
    return "Unknown Technique";
}

/// Returns Sudoku Explainer (SE) difficulty rating for technique.
/// Ratings based on the SE scale by Nicolas Juillerat (Sudoku Explainer v1.2.1).
/// See: https://github.com/SudokuMonster/SukakuExplainer/wiki/Difficulty-ratings-in-Sudoku-Explainer-v1.2.1
/// Techniques without a direct SE equivalent use ratings consistent with the SE scale.
/// @param technique The solving technique
/// @return SE difficulty rating (0.0 for unknown, 1.0-12.0 for known techniques)
[[nodiscard]] constexpr double getTechniqueRating(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        case HiddenSingle:
            return 1.5;  // SE: 1.2 (block) / 1.5 (row/col)
        case NakedSingle:
            return 2.3;  // SE v1.2.1
        case PointingPair:
            return 2.6;  // SE "Pointing"
        case BoxLineReduction:
            return 2.8;  // SE "Claiming"
        case NakedPair:
            return 3.0;  // SE v1.2.1
        case XWing:
            return 3.2;    // SE v1.2.1
        case FinnedXWing:  // SE-compatible: X-Wing + fin overhead
        case HiddenPair:   // SE v1.2.1
            return 3.4;
        case NakedTriple:
            return 3.6;  // SE v1.2.1
        case Swordfish:
            return 3.8;        // SE v1.2.1
        case HiddenTriple:     // SE v1.2.1
        case SimpleColoring:   // SE-compatible: simple single-digit chain
        case Skyscraper:       // SE-compatible: Turbot Fish equivalent
        case FinnedSwordfish:  // SE-compatible: Swordfish + fin
            return 4.0;
        case TwoStringKite:
            return 4.1;      // SE-compatible: slightly harder than Skyscraper
        case MultiColoring:  // SE-compatible: cross-cluster coloring
        case FrankenFish:    // SE-compatible: mixed base/cover fish
        case XYWing:         // SE v1.2.1
            return 4.2;
        case ThreeDMedusa:  // SE-compatible: multi-digit coloring
        case WWing:         // SE-compatible: XYZ-Wing equivalent
        case XYZWing:       // SE v1.2.1
            return 4.4;
        case UniqueRectangle:     // SE v1.2.1 (Type 1; Types 2-4 up to 5.0)
        case AvoidableRectangle:  // SE-compatible: UR Type 1 equivalent
        case EmptyRectangle:      // SE-compatible: single-digit intersection
        case RemotePairs:         // SE-compatible: chaining technique
            return 4.5;
        case WXYZWing:
            return 4.6;              // SE-compatible: 4-cell wing
        case HiddenUniqueRectangle:  // SE-compatible: harder UR variant
        case VWXYZWing:              // SE-compatible: 5-cell wing
            return 4.8;
        case NakedQuad:
            return 5.0;  // SE v1.2.1
        case Jellyfish:
            return 5.2;        // SE v1.2.1
        case HiddenQuad:       // SE v1.2.1
        case FinnedJellyfish:  // SE-compatible: Jellyfish + fin
            return 5.4;
        case BUG:
            return 5.6;  // SE v1.2.1
        case XCycles:    // SE: X-chains/X-cycles (6.5-6.9)
        case XYChain:    // SE: Y-cycles (6.6-7.0)
            return 6.6;
        case GroupedXCycles:
            return 6.8;     // SE-compatible: grouped nodes add complexity
        case ALSxZ:         // SE-compatible: forcing chain level
        case SueDeCoq:      // SE-compatible: complex disjoint subsets
        case ForcingChain:  // SE v1.2.1 (7.1-7.5 range)
        case NiceLoop:      // SE-compatible: AIC equivalent
            return 7.5;
        case ALSXYWing:
            return 7.8;  // SE-compatible: three ALS linked
        case DeathBlossom:
            return 8.2;  // SE-compatible: region forcing chain level
        case Backtracking:
            return 12.0;  // Maximum: brute force / trial-and-error
    }
    return 0.0;
}

/// Returns localized technique name using the localization manager.
/// @param loc Localization manager for string lookup
/// @param technique The solving technique
/// @return Localized technique name as string_view (e.g., "Naked Single" in English)
[[nodiscard]] inline std::string_view getLocalizedTechniqueName(const ILocalizationManager& loc,
                                                                SolvingTechnique technique) {
    using enum SolvingTechnique;
    using namespace StringKeys;
    switch (technique) {
        case NakedSingle:
            return loc.getString(TechNakedSingle);
        case HiddenSingle:
            return loc.getString(TechHiddenSingle);
        case NakedPair:
            return loc.getString(TechNakedPair);
        case NakedTriple:
            return loc.getString(TechNakedTriple);
        case HiddenPair:
            return loc.getString(TechHiddenPair);
        case HiddenTriple:
            return loc.getString(TechHiddenTriple);
        case PointingPair:
            return loc.getString(TechPointingPair);
        case BoxLineReduction:
            return loc.getString(TechBoxLineReduction);
        case NakedQuad:
            return loc.getString(TechNakedQuad);
        case HiddenQuad:
            return loc.getString(TechHiddenQuad);
        case XWing:
            return loc.getString(TechXWing);
        case XYWing:
            return loc.getString(TechXYWing);
        case Swordfish:
            return loc.getString(TechSwordfish);
        case Skyscraper:
            return loc.getString(TechSkyscraper);
        case TwoStringKite:
            return loc.getString(TechTwoStringKite);
        case XYZWing:
            return loc.getString(TechXYZWing);
        case UniqueRectangle:
            return loc.getString(TechUniqueRectangle);
        case WWing:
            return loc.getString(TechWWing);
        case SimpleColoring:
            return loc.getString(TechSimpleColoring);
        case FinnedXWing:
            return loc.getString(TechFinnedXWing);
        case RemotePairs:
            return loc.getString(TechRemotePairs);
        case BUG:
            return loc.getString(TechBUG);
        case Jellyfish:
            return loc.getString(TechJellyfish);
        case FinnedSwordfish:
            return loc.getString(TechFinnedSwordfish);
        case EmptyRectangle:
            return loc.getString(TechEmptyRectangle);
        case WXYZWing:
            return loc.getString(TechWXYZWing);
        case FinnedJellyfish:
            return loc.getString(TechFinnedJellyfish);
        case XYChain:
            return loc.getString(TechXYChain);
        case MultiColoring:
            return loc.getString(TechMultiColoring);
        case ALSxZ:
            return loc.getString(TechALSxZ);
        case SueDeCoq:
            return loc.getString(TechSueDeCoq);
        case ForcingChain:
            return loc.getString(TechForcingChain);
        case NiceLoop:
            return loc.getString(TechNiceLoop);
        case XCycles:
            return loc.getString(TechXCycles);
        case ThreeDMedusa:
            return loc.getString(TechThreeDMedusa);
        case HiddenUniqueRectangle:
            return loc.getString(TechHiddenUniqueRectangle);
        case AvoidableRectangle:
            return loc.getString(TechAvoidableRectangle);
        case ALSXYWing:
            return loc.getString(TechALSXYWing);
        case DeathBlossom:
            return loc.getString(TechDeathBlossom);
        case VWXYZWing:
            return loc.getString(TechVWXYZWing);
        case FrankenFish:
            return loc.getString(TechFrankenFish);
        case GroupedXCycles:
            return loc.getString(TechGroupedXCycles);
        case Backtracking:
            return loc.getString(TechBacktrackingName);
    }
    return loc.getString(TechUnknown);
}

}  // namespace sudoku::core
