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

#include "core/i18n_helpers.h"
#include "solving_technique.h"

#include <string>

namespace sudoku::core {

/// Localized theory text for a solving technique
struct TechniqueDescription {
    std::string title;
    std::string what_it_is;
    std::string what_to_look_for;
};

/// Get the localized description for a solving technique
/// @param technique The technique to describe
/// @return Description with localized title, explanation, and identification tips
// NOLINTNEXTLINE(readability-function-size) — dispatch table over all SolvingTechnique values; each case returns one constant struct; cannot meaningfully split
[[nodiscard]] inline TechniqueDescription getTechniqueDescription(SolvingTechnique technique) {
    using enum SolvingTechnique;
    switch (technique) {
        case NakedSingle:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is =
                    core::loc("Sudoku", "A cell has only one possible candidate left. All other values are eliminated "
                                        "by row, column, and box constraints."),
                .what_to_look_for = core::loc(
                    "Sudoku",
                    "Look for cells where 8 of the 9 values are already present in the cell's row, column, or box.")};
        case HiddenSingle:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A value can only go in one cell within a row, column, or box. Even though the cell "
                                  "may have multiple candidates, only this value has no other place in the region."),
                    .what_to_look_for =
                        core::loc("Sudoku", "For each region, check if any value has only one possible cell.")};
        case NakedPair:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "Two cells in the same region each contain exactly the same two candidates. Those two values "
                        "must go in those two cells, so they can be eliminated from all other cells in the region."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find two cells in a row, column, or box that share the same pair of candidates.")};
        case NakedTriple:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "Three cells in a region collectively contain exactly three candidates. Each cell has a subset "
                        "of those three values. Those values can be eliminated from other cells in the region."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Find three cells in a region whose combined candidates form a set of exactly three values.")};
        case HiddenPair:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc("Sudoku", "Two values in a region appear as candidates in exactly the same two "
                                                  "cells. Other candidates in those two cells can be eliminated."),
                .what_to_look_for =
                    core::loc("Sudoku", "For each region, find two values that appear only in the same two cells.")};
        case HiddenTriple:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "Three values in a region appear as candidates in exactly three cells. "
                                            "Other candidates in those cells can be eliminated."),
                    .what_to_look_for =
                        core::loc("Sudoku", "For each region, find three values confined to exactly three cells.")};
        case PointingPair:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A candidate in a box is confined to a single row or column. That candidate can be "
                                  "eliminated from the rest of that row or column outside the box."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "In each box, check if a candidate appears only in one row or one column.")};
        case BoxLineReduction:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A candidate in a row or column is confined to a single box. That candidate "
                                  "can be eliminated from the rest of the box outside that row or column."),
                    .what_to_look_for =
                        core::loc("Sudoku", "In each row/column, check if a candidate appears only within one box.")};
        case NakedQuad:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "Four cells in a region collectively contain exactly four candidates. "
                                            "Those values can be eliminated from other cells in the region."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Find four cells in a region whose combined candidates form a set of exactly four values.")};
        case HiddenQuad:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "Four values in a region appear as candidates in exactly four cells. Other "
                                            "candidates in those cells can be eliminated."),
                    .what_to_look_for =
                        core::loc("Sudoku", "For each region, find four values confined to exactly four cells.")};
        case XWing:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A candidate appears in exactly two cells in each of two rows, and those cells are in the same "
                        "two columns. The candidate can be eliminated from other cells in those columns."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find a candidate forming a rectangle pattern: two rows, two columns, four cells.")};
        case XYWing:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A pivot cell with candidates {A,B} sees two wing cells: one with {A,C} and one with "
                                  "{B,C}. Value C can be eliminated from any cell that sees both wings."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Find a bivalue cell (pivot) that sees two other bivalue cells sharing one candidate each.")};
        case Swordfish:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A candidate appears in 2-3 cells in each of three rows, and those cells fall in exactly three "
                        "columns. The candidate can be eliminated from other cells in those columns."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Extend the X-Wing pattern to three rows and three columns.")};
        case Skyscraper:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "Two conjugate pairs for a digit share one endpoint in the same row or column. The "
                                  "digit can be eliminated from cells that see both non-shared endpoints."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Find two rows (or columns) each with exactly two cells for a digit, "
                                            "sharing one column (or row).")};
        case TwoStringKite:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A conjugate pair in a row and a conjugate pair in a column are connected through a box. The "
                        "digit can be eliminated from the cell that sees both unconnected endpoints."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find a row pair and column pair for the same digit connected via a shared box.")};
        case XYZWing:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is =
                    core::loc("Sudoku", "A pivot cell with candidates {A,B,C} sees a wing with {A,B} and a wing with "
                                        "{A,C}. Value A can be eliminated from cells that see all three cells."),
                .what_to_look_for = core::loc(
                    "Sudoku",
                    "Find a trivalue pivot seeing two bivalue wings that each share two candidates with the pivot.")};
        case UniqueRectangle:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "Four cells forming a rectangle across two boxes would create a deadly "
                                            "pattern (two solutions) if they all had the same two candidates. Extra "
                                            "candidates in some cells can force eliminations to avoid this ambiguity."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find four cells in a rectangle across two boxes sharing the same two candidates.")};
        case WWing:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "Two cells with the same pair of candidates {A,B} are connected by a strong link on "
                                  "value A. Value B can be eliminated from cells that see both endpoints."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Find two identical bivalue cells connected by a conjugate pair on one of their values.")};
        case SimpleColoring:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku",
                    "For a single digit, build chains of conjugate pairs and assign two colors. If both colors "
                    "appear in the same region, one color is false and its candidates are eliminated."),
                .what_to_look_for = core::loc(
                    "Sudoku", "Pick a digit, trace conjugate pairs, color alternately. Check for color conflicts.")};
        case FinnedXWing:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku", "An X-Wing pattern with one extra candidate cell (the fin) in the same box as a corner. "
                              "Eliminations are restricted to cells that see both the fin and the X-Wing column."),
                .what_to_look_for =
                    core::loc("Sudoku", "Find an X-Wing where one row has an extra candidate cell in the same box.")};
        case RemotePairs:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A chain of bivalue cells all containing the same pair {A,B}, where each adjacent pair shares "
                        "a region. Cells seeing both endpoints of an even-length chain lose both values."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find a chain of identical bivalue cells connected through shared regions.")};
        case BUG:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is =
                    core::loc("Sudoku", "If all unsolved cells have exactly two candidates except one cell with "
                                        "three, the puzzle would have multiple solutions unless the trivalue cell "
                                        "is set to the value that appears three times in its row, column, or box."),
                .what_to_look_for = core::loc(
                    "Sudoku", "Check if only one cell has more than two candidates. If so, find its odd-count value.")};
        case Jellyfish:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A candidate appears in 2-4 cells in each of four rows, and those cells fall in exactly four "
                        "columns. The candidate can be eliminated from other cells in those columns."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Extend the Swordfish pattern to four rows and four columns.")};
        case FinnedSwordfish:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A Swordfish pattern with extra candidate cells (fins) in the same box. Eliminations "
                                  "are restricted to cells seeing both the fin box and the Swordfish columns."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Find a Swordfish where one row has extra candidates in the same box.")};
        case EmptyRectangle:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku",
                    "A digit's candidates in a box form an L-shape or cross, leaving an empty rectangle. Combined with "
                    "a conjugate pair outside the box, this eliminates the digit from a target cell."),
                .what_to_look_for = core::loc(
                    "Sudoku",
                    "Find a box where a digit's candidates leave an empty rectangle, connected to a conjugate pair.")};
        case WXYZWing:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A four-cell wing pattern: a pivot and three wings collectively contain four candidates, and a "
                        "shared candidate Z can be eliminated from cells seeing all cells containing Z."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Find a group of four cells with exactly four combined candidates sharing a common value.")};
        case FinnedJellyfish:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A Jellyfish pattern with extra fin cells in the same box. Eliminations are "
                                  "restricted to cells seeing both the fin box and the Jellyfish columns."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Find a Jellyfish where one row has extra candidates forming a fin.")};
        case XYChain:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is =
                    core::loc("Sudoku", "A chain of bivalue cells where consecutive cells share a candidate value, "
                                        "alternating between the two candidates. The value shared by the chain's "
                                        "endpoints can be eliminated from cells that see both endpoints."),
                .what_to_look_for = core::loc(
                    "Sudoku", "Build a chain of bivalue cells connected by shared candidates. Check the endpoints.")};
        case MultiColoring:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku", "Build separate conjugate pair chains (clusters) for a digit and color each. When "
                              "two clusters interact (cells in different clusters see each other), eliminations "
                              "can be made from cells that see conflicting colors across clusters."),
                .what_to_look_for = core::loc(
                    "Sudoku", "Color multiple conjugate chains for one digit, then check cross-cluster interactions.")};
        case ALSxZ:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku",
                    "Two Almost Locked Sets (each has N cells with N+1 candidates) share a restricted common candidate "
                    "X. A second common candidate Z can be eliminated from cells that see all Z-cells in both sets."),
                .what_to_look_for = core::loc(
                    "Sudoku",
                    "Find two groups of cells that are almost locked, sharing a restricted common candidate.")};
        case SueDeCoq:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "An intersection of a line and box contains 2-3 cells whose candidates can be covered by two "
                        "Almost Locked Sets (one from the line remainder, one from the box remainder). Extra ALS "
                        "candidates can be eliminated from their respective remainders."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find an intersection where candidates can be partitioned into two covering ALS.")};
        case ForcingChain:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "For a cell with 2-3 candidates, assume each candidate is true and "
                                            "propagate the consequences. If all assumptions lead to the same "
                                            "conclusion (a placement or elimination), that conclusion must be true."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Pick a cell with few candidates. Try each value and propagate. Look for common outcomes.")};
        case NiceLoop:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "Build a chain of alternating strong and weak links between (cell, digit) "
                                            "pairs. If the chain forms a loop or its endpoints share a digit, "
                                            "eliminations can be derived from the alternating inference chain rules."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Trace alternating strong/weak links. Check if endpoints share a digit for eliminations.")};
        case XCycles:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "For a single digit, build a chain of alternating strong and weak links. Type 1 (continuous "
                        "loop) eliminates the digit from cells seeing weak link endpoints. Type 2 places the digit at "
                        "a strong-strong discontinuity. Type 3 eliminates at a weak-weak discontinuity."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "For each digit, trace alternating strong/weak links and look for cycles or discontinuities.")};
        case ThreeDMedusa:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "Multi-digit coloring: build a graph of (cell, digit) pairs connected by strong "
                                  "links (conjugate pairs and bivalue cells). Color with two alternating colors. Apply "
                                  "six rules to find contradictions or trap eliminations."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Extend single-digit coloring to multiple digits via bivalue cell connections.")};
        case HiddenUniqueRectangle:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "A deadly rectangle pattern where one or more corners have the UR values "
                                            "hidden among other candidates. Strong links on UR values in shared units "
                                            "force eliminations to avoid the deadly pattern."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Find a rectangle across two boxes where the UR values are present but hidden by extras.")};
        case AvoidableRectangle:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is =
                    core::loc("Sudoku", "Like Unique Rectangle but using the distinction between given clues and "
                                        "solver-placed values. If three solver-placed corners of a rectangle have "
                                        "values {A,B}, the fourth unsolved corner cannot complete the deadly pattern."),
                .what_to_look_for = core::loc(
                    "Sudoku", "Find rectangles where three corners are solver-placed (not givens) with two values.")};
        case ALSXYWing:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "Three non-overlapping Almost Locked Sets linked by restricted commons: "
                                            "A-B linked by X, B-C linked by Y (Y != X). Common value Z in both A and C "
                                            "can be eliminated from cells seeing all Z-cells in both A and C."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Find three ALSs forming a chain with two restricted common candidates.")};
        case DeathBlossom:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "A stem cell with 2-3 candidates, each linked to a petal ALS via "
                                            "restricted common. A value Z common across all petals (but not in the "
                                            "stem) can be eliminated from cells seeing all Z-cells in all petals."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find a cell whose candidates each connect to an ALS via restricted common.")};
        case VWXYZWing:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A five-cell wing pattern: a pivot and four wings collectively contain five candidates. The "
                        "non-restricted shared value Z can be eliminated from cells seeing all Z-cells."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Find five cells with exactly five combined candidates and a restricted elimination value.")};
        case FrankenFish:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku",
                    "A fish pattern (X-Wing/Swordfish/Jellyfish) where base and cover sets are mixed rows/columns and "
                    "boxes. At least one base set must be a box. Eliminates from cover cells outside the base."),
                .what_to_look_for =
                    core::loc("Sudoku", "Look for fish patterns that include boxes as base or cover sets.")};
        case GroupedXCycles:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "Extends X-Cycles by allowing grouped nodes: 2-3 cells in the same box on the same "
                                  "row or column that all have a candidate digit. Same Type 1/2/3 rules apply."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Build X-Cycle chains using grouped box nodes alongside individual cells.")};
        case SashimiXWing:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A fish pattern where one base row has only one candidate position instead of two. The missing "
                        "position is compensated by a fin cell, restricting eliminations to the fin's box."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Look for an X-Wing-like pattern where one row is incomplete — it only has the "
                                  "candidate in one of the two expected columns, plus an extra fin cell.")};
        case SashimiSwordfish:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku", "A 3-row fish pattern where at least one base row has fewer candidate positions than "
                              "expected. The missing position creates a fin that restricts eliminations."),
                .what_to_look_for = core::loc(
                    "Sudoku", "Find a Swordfish shape where one row only covers 1 of the 3 base columns, plus a fin.")};
        case SashimiJellyfish:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku", "A 4-row fish pattern where at least one base row has fewer candidate positions than "
                              "expected. The missing position creates a fin restricting eliminations."),
                .what_to_look_for = core::loc(
                    "Sudoku", "Find a Jellyfish shape where one row only covers 1 of the 4 base columns, plus a fin.")};
        case UnitForcingChain:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc(
                    "Sudoku", "For a digit in a unit with 2-3 positions, assume the digit goes in each position and "
                              "propagate. If all branches lead to the same conclusion, that conclusion is true."),
                .what_to_look_for =
                    core::loc("Sudoku", "Find a unit where a digit appears in few cells, then try each placement.")};
        case RegionForcingChain:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "For a digit in a box with 2-3 positions, assume the digit goes in each position and "
                                  "propagate. If all branches lead to the same conclusion, that conclusion is true."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Find a box where a digit appears in few cells, then try each placement.")};
        case MutantFish:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A fish pattern where BOTH base and cover sets freely mix rows, columns, and boxes. "
                                  "Unlike Franken Fish (one mixed side), Mutant Fish requires both sides to contain at "
                                  "least 2 different unit types. Eliminates from cover cells outside the base."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Look for fish patterns where both the base set and cover set mix rows, columns, and boxes.")};
        case KrakenFish:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "Extends finned fish by using chain propagation to verify eliminations outside the fin's "
                        "box. For each candidate that a standard finned fish would reject (outside the fin's "
                        "box), place the digit at the fin cell and propagate. If the target still loses the "
                        "candidate, the elimination is valid regardless of whether the fin is true or false."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Find a finned fish pattern, then check if chain propagation from the "
                                            "fin cell eliminates the digit from cells outside the fin's box.")};
        case ALSChain:
            return {
                .title = getLocalizedTechniqueName(technique),
                .what_it_is = core::loc("Sudoku", "A generalized chain of 4-6 Almost Locked Sets linked by distinct "
                                                  "restricted commons. Values common across the chain endpoints can be "
                                                  "eliminated from cells that see all relevant ALS members."),
                .what_to_look_for =
                    core::loc("Sudoku", "Find a chain of ALSs where each adjacent pair shares a restricted common.")};
        case UniqueLoop:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "A deadly pattern where 4-6 cells form a loop, each consecutive pair sharing a unit "
                        "(row, column, or box). All cells contain the same candidate pair {A,B}. If all cells "
                        "had only {A,B}, two solutions would exist. Cells with extra candidates must keep them."),
                    .what_to_look_for =
                        core::loc("Sudoku", "Find a loop of 4-6 cells across at least 2 boxes where each cell has "
                                            "candidates {A,B} and each consecutive pair shares a row, column, or "
                                            "box. If exactly one cell has extras, eliminate A and B from it.")};
        case JuniorExocet:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "A base pair of cells in a box whose candidates must appear in specific "
                                            "target cells in other boxes along cross-lines. Candidates not matching "
                                            "the base pair pattern can be eliminated from the target cells."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Find a base pair in a box with target cells in aligned boxes along cross-lines.")};
        case ContinuousNiceLoop:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku", "A continuous alternating inference chain (AIC) that forms a complete loop. Every "
                                  "weak link in the loop produces eliminations: the digit can be removed from any cell "
                                  "outside the loop that sees both endpoints of a weak link."),
                    .what_to_look_for = core::loc(
                        "Sudoku",
                        "Build an AIC where the chain closes back on itself with consistent alternating links. Every "
                        "weak link segment yields eliminations from external cells seeing both endpoints.")};
        case GroupedNiceLoop:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is = core::loc(
                        "Sudoku",
                        "Extends Nice Loop (AIC) with grouped nodes: 2-3 cells in the same box on the same row or "
                        "column that all share a candidate digit. Combines multi-digit cell-based links (bivalue "
                        "strong links) with single-digit grouped unit links for stronger chains."),
                    .what_to_look_for = core::loc(
                        "Sudoku", "Build AIC chains using grouped box nodes alongside individual cells. Look for "
                                  "discontinuous Type 2 chains where both endpoints assert the same digit.")};
        case Backtracking:
            return {.title = getLocalizedTechniqueName(technique),
                    .what_it_is =
                        core::loc("Sudoku", "A brute-force trial-and-error method. Not a logical technique — used as a "
                                            "fallback when no logical strategy can make progress."),
                    .what_to_look_for = core::loc("Sudoku", "This technique is not used in training exercises.")};
    }
    return {.title = getLocalizedTechniqueName(technique),
            .what_it_is = core::loc("Sudoku", "Unknown technique."),
            .what_to_look_for = core::loc("Sudoku", "No identification tips available.")};
}

}  // namespace sudoku::core
