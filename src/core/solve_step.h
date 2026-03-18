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

/// Region type for solving step explanations
enum class RegionType : std::int8_t {
    Row = 0,
    Col = 1,
    Box = 2,
    None = -1
};

/// Structured data for localized explanation templates.
/// Strategies populate this alongside the English explanation string.
struct ExplanationData {
    std::vector<Position>
        positions{};            // NOLINT(readability-redundant-member-init) - Suppresses -Wmissing-field-initializers
    std::vector<int> values{};  // NOLINT(readability-redundant-member-init) - Suppresses -Wmissing-field-initializers
    RegionType region_type{RegionType::None};            ///< Region involved (Row/Col/Box)
    size_t region_index{0};                              ///< Region index (0-indexed)
    RegionType secondary_region_type{RegionType::None};  ///< For techniques spanning two regions
    size_t secondary_region_index{0};                    ///< Secondary region index
    int technique_subtype{-1};                           ///< Technique-specific variant (UR type, coloring mode, etc.)
    std::vector<CellRole>
        position_roles{};  // NOLINT(readability-redundant-member-init) - Suppresses -Wmissing-field-initializers

    bool operator==(const ExplanationData& other) const = default;
};

/// Represents a single step in solving a Sudoku puzzle
/// Contains either a placement or a list of candidate eliminations
struct SolveStep {
    SolveStepType type;                     ///< Type of step (placement or elimination)
    SolvingTechnique technique;             ///< Technique used to derive this step
    Position position;                      ///< For placements: cell to fill; for eliminations: unused
    int value;                              ///< For placements: value to place; for eliminations: unused
    std::vector<Elimination> eliminations;  ///< For eliminations: candidates to remove
    std::string explanation;                ///< Human-readable explanation of the step
    double rating;                          ///< Sudoku Explainer (SE) difficulty rating
    ExplanationData explanation_data;       ///< Structured data for localized explanations

    bool operator==(const SolveStep& other) const = default;
};

}  // namespace sudoku::core
