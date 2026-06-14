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

#include "i_game_validator.h"  // For Position
#include "solving_technique.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sudoku::core {

/// Type of solving step (placement or elimination)
enum class SolveStepType : std::uint8_t {
    Placement,   ///< Places a value in a cell
    Elimination  ///< Eliminates candidates from cells
};

/// Represents elimination of a candidate from a cell
struct Elimination {
    Position position;  ///< Cell position to eliminate from
    int value{0};       ///< Candidate value to eliminate (1-9)

    bool operator==(const Elimination& other) const = default;
};

/// Role of a cell in a solving technique's visual explanation.
/// Used by Training Mode to color-code cells on the board.
enum class CellRole : uint8_t {
    Pattern,        ///< General pattern cell (default)
    Pivot,          ///< Pivot cell in wing techniques (XY-Wing, XYZ-Wing, etc.)
    Wing,           ///< Wing cell in wing techniques
    Fin,            ///< Fin cell in finned fish techniques
    Roof,           ///< Roof cell in Unique Rectangle
    Floor,          ///< Floor cell in Unique Rectangle
    ChainA,         ///< Chain color A (coloring, remote pairs, XY-chain)
    ChainB,         ///< Chain color B
    LinkEndpoint,   ///< Endpoint of a strong/weak link
    SetA,           ///< Set A in ALS-XZ or Sue de Coq
    SetB,           ///< Set B in ALS-XZ or Sue de Coq
    SetC,           ///< Set C in ALS-XY-Wing or Death Blossom
    CorrectAnswer,  ///< Feedback: player's correct elimination/placement
    WrongAnswer,    ///< Feedback: player's incorrect elimination/placement
    MissedAnswer    ///< Feedback: expected answer the player missed
};

/// A cell position paired with its visual role in a technique explanation
struct CellHighlight {
    Position position;
    CellRole role{CellRole::Pattern};

    bool operator==(const CellHighlight& other) const = default;
};

/// Region type for solving step explanations
enum class RegionType : std::int8_t {
    Row = 0,
    Col = 1,
    Box = 2,
    None = -1
};

/// Per-step context that lets getTechniqueRating() vary *within* a single technique (Story 0b.4a).
///
/// The flat getTechniqueRating(SolvingTechnique) in solving_technique.h is the base/default rating;
/// this struct carries the signals Sudoku Explainer / the SukakuExplainer fork use to rate the
/// *individual step*: the resolving region (Class A — block vs line), whether the step forces a
/// placement (Class B), and the pattern size/length (Class C). Defaults reproduce the flat rating, so
/// getTechniqueRating(t, RatingContext{}) == getTechniqueRating(t) for every technique.
///
/// Class A is live now; `forces_placement` / `size_or_length` are carried but not yet consumed —
/// 0b-4b (Direct variants / Full House) and 0b-4c (size/length scaling) wire them up.
struct RatingContext {
    RegionType region{RegionType::None};  ///< Resolving region (Class A: Box → block, Row/Col → line)
    bool forces_placement{false};         ///< Class B: the placement-forcing "Direct" form (0b-4b)
    int size_or_length{0};                ///< Class C: fish size / chain length (0b-4c)

    bool operator==(const RatingContext& other) const = default;
};

/// Class C (Story 0b.4c) — base size/length at which the scaled value equals the flat base (+0).
/// Fish (FrankenFish/MutantFish/KrakenFish) are smallest at size 2; chains/loops (XYChain/XCycles/
/// GroupedXCycles + the AIC nice-loop family) are shortest at length 4 (XYChain needs depth ≥ 3).
inline constexpr int kFishBaseSize = 2;
inline constexpr int kChainBaseLength = 4;

/// Class C structural scaling (Story 0b.4c): a larger fish / longer chain rates strictly higher than a
/// smaller/shorter one of the same technique. `flat_base` is the rating at the canonical `base_unit`;
/// every extra unit of size/length adds one tenth (0.1). The arithmetic is done in integer tenths so the
/// result stays an EXACT decimal multiple of 0.1 — `4.2 + 0.1` drifts off `4.3` in IEEE doubles, which
/// would break the golden pin's `==`. Below the base unit (and at the `size_or_length == 0` sentinel) the
/// clamp returns the flat base unchanged, so default context reproduces the flat rating exactly (AC4b).
[[nodiscard]] constexpr double scaledByStructure(double flat_base, int size_or_length, int base_unit) {
    constexpr double kTenthsPerUnit = 10.0;  // ratings are 0.1-multiples; tenths keep results exact for ==
    constexpr double kRoundHalf = 0.5;       // round-to-nearest when recovering integer tenths from the base
    if (size_or_length <= base_unit) {
        return flat_base;
    }
    const int base_tenths = static_cast<int>((flat_base * kTenthsPerUnit) + kRoundHalf);
    const int scaled_tenths = base_tenths + (size_or_length - base_unit);  // +1 tenth per unit above base
    return static_cast<double>(scaled_tenths) / kTenthsPerUnit;
}

/// Context-sensitive SE rating (Story 0b.4a) — the authoritative single source for ratings that vary
/// by step context. Delegates to the flat getTechniqueRating(technique) for the base value and only
/// overrides per class, so the per-technique numbers live in exactly one place.
///
/// Class A — Hidden Single region split (SE v1.2.1): a single hidden in a *block* is the easiest case
/// (1.2); a single hidden only in a *line* (row/col) rates 1.5. Block precedence ("easiest region
/// wins") matches SE: when a cell is a hidden single in its box, it is reported as the 1.2 block case.
///
/// Class B — "Direct" forms (SE v1.2.1, story 0b.4b): Pointing/Claiming/Hidden-subset steps whose own
/// eliminations force a placement (`forces_placement`) rate lower than the elimination-only form —
/// Direct Pointing 1.7 (vs 2.6), Direct Claiming 1.9 (vs 2.8), Direct Hidden Pair 2.0 (vs 3.4), Direct
/// Hidden Triplet 2.5 (vs 4.0). Full House (1.0) is a distinct technique with a flat rating, not a flag.
///
/// Class C — structural size/length scaling (story 0b.4c): a larger fish / longer chain rates strictly
/// higher than a smaller/shorter one of the same technique, via `scaledByStructure()`. The X-chain/cycle
/// families stay inside Expert *by construction* — XYChain/XCycles/GroupedXCycles top out at 7.1/7.2/7.4
/// (< 7.5) at their max generated length. The AIC nice-loop family is also scaled (per design), and there
/// ContinuousNiceLoop (base 7.0) could in principle reach Master for a very long loop (7.0 + 0.1·(9−4) =
/// 7.5); that crossing is NOT closed by construction but is empirically inert — a nice-loop is never a
/// generated puzzle's hardest step, and the fixed-seed bucket-migration corpus confirms 0 reclassifications
/// (test_class_c_bucket_migration_integration.cpp). NiceLoop/GroupedNiceLoop (7.5/8.0) are already Master.
[[nodiscard]] constexpr double getTechniqueRating(SolvingTechnique technique, const RatingContext& context) {
    if (technique == SolvingTechnique::HiddenSingle && context.region == RegionType::Box) {
        return 1.2;  // block hidden single (line value 1.5 stays in the flat base overload)
    }
    if (context.forces_placement) {
        switch (technique) {
            case SolvingTechnique::PointingPair:
                return 1.7;  // SE "Direct Pointing"
            case SolvingTechnique::BoxLineReduction:
                return 1.9;  // SE "Direct Claiming"
            case SolvingTechnique::HiddenPair:
                return 2.0;  // SE "Direct Hidden Pair"
            case SolvingTechnique::HiddenTriple:
                return 2.5;  // SE "Direct Hidden Triplet"
            default:
                break;
        }
    }
    switch (technique) {
        case SolvingTechnique::FrankenFish:
        case SolvingTechnique::MutantFish:
        case SolvingTechnique::KrakenFish:
            return scaledByStructure(getTechniqueRating(technique), context.size_or_length, kFishBaseSize);
        case SolvingTechnique::XYChain:
        case SolvingTechnique::XCycles:
        case SolvingTechnique::GroupedXCycles:
        case SolvingTechnique::NiceLoop:
        case SolvingTechnique::ContinuousNiceLoop:
        case SolvingTechnique::GroupedNiceLoop:
            return scaledByStructure(getTechniqueRating(technique), context.size_or_length, kChainBaseLength);
        default:
            break;
    }
    return getTechniqueRating(technique);
}

/// Structured data for localized explanation templates.
/// Strategies populate this alongside the English explanation string.
struct ExplanationData {
    std::vector<Position>
        positions{};            // NOLINT(readability-redundant-member-init) - Suppresses -Wmissing-field-initializers
    std::vector<int> values{};  // NOLINT(readability-redundant-member-init) - Suppresses -Wmissing-field-initializers
    RegionType region_type{RegionType::None};  ///< A single *named* region (paired with region_index), used by
                                               ///< non-fish techniques (Naked/Hidden Pair/Triple/Quad, PointingPair,
                                               ///< BoxLineReduction, EmptyRectangle) for "Row 3"/"Box 7" rendering.
                                               ///< NOT a fish base axis — see pattern_axis. (gh#39)
    size_t region_index{0};                    ///< Region index (0-indexed); only meaningful alongside region_type.
    RegionType secondary_region_type{RegionType::None};  ///< For techniques spanning two regions
    size_t secondary_region_index{0};                    ///< Secondary region index
    RegionType pattern_axis{
        RegionType::None};  ///< Base-axis orientation of a fish pattern (Row when bases are rows,
                            ///< Col when bases are columns). Drives fish localized-template
                            ///< selection; replaces the former overloaded use of region_type. (gh#39)
    RegionType elimination_axis{RegionType::None};  ///< Axis/region where this technique's eliminations land: the
                                                    ///< perpendicular line for basic fish, Box for finned/sashimi,
                                                    ///< None when not a single axis (e.g. Kraken chain eliminations).
    // TODO(gh#39): elimination_axis is the load-bearing hook for a future fish-line / elimination-region highlight
    // in src/view/; owner = maintainer until that feature is scheduled. No consumer reads it yet.
    int technique_subtype{-1};  ///< Technique-specific variant (UR type, coloring mode, etc.)
    std::vector<CellRole>
        position_roles{};  // NOLINT(readability-redundant-member-init) - Suppresses -Wmissing-field-initializers

    bool operator==(const ExplanationData& other) const = default;
};

/// Represents a single step in solving a Sudoku puzzle
/// Contains either a placement or a list of candidate eliminations
struct SolveStep {
    SolveStepType type{};                   ///< Type of step (placement or elimination)
    SolvingTechnique technique{};           ///< Technique used to derive this step
    Position position;                      ///< For placements: cell to fill; for eliminations: unused
    int value{};                            ///< For placements: value to place; for eliminations: unused
    std::vector<Elimination> eliminations;  ///< For eliminations: candidates to remove
    std::string explanation;                ///< Human-readable explanation of the step
    double rating{};                        ///< Sudoku Explainer (SE) difficulty rating
    ExplanationData explanation_data;       ///< Structured data for localized explanations

    bool operator==(const SolveStep& other) const = default;
};

}  // namespace sudoku::core
