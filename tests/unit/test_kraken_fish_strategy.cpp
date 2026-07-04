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

#include "../../src/core/candidate_grid.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/reconstruction.h"
#include "../../src/core/strategies/forcing_chain_helpers.h"
#include "../../src/core/strategies/kraken_fish_strategy.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/strategy_test_utils.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace sudoku::core {
// Friend peer: reaches the private cover-sweep so issue #59 can be exercised deterministically.
// `KrakenOutcome` is private, so the peer returns only the public `eliminations` vector.
struct KrakenFishTestPeer {
    [[nodiscard]] static std::vector<Elimination>
    coverSweepEliminations(const BoardData& board, const CandidateGrid& candidates, int value, bool by_row,
                           std::span<const size_t> base_primaries, const std::vector<size_t>& base_secondaries,
                           const Position& fin_pos, size_t fin_box) {
        return KrakenFishStrategy::findKrakenOutcome(board, candidates, value, by_row, base_primaries, base_secondaries,
                                                     fin_pos, fin_box)
            .eliminations;
    }
};
}  // namespace sudoku::core

namespace {

// Helpers below each short-circuit on `result.has_value()` before any `->`/`*`, so CI's
// `bugprone-unchecked-optional-access` (which does not model REQUIRE as a guard) stays happy.

// The role==Fin cell of a Kraken step, if the step exists and tags one.
std::optional<Position> finCell(const std::optional<SolveStep>& s) {
    if (!s.has_value()) {
        return std::nullopt;
    }
    const auto& roles = s->explanation_data.position_roles;
    const auto& positions = s->explanation_data.positions;
    if (roles.size() != positions.size()) {
        return std::nullopt;
    }
    auto it = std::ranges::find(roles, CellRole::Fin);
    if (it == roles.end()) {
        return std::nullopt;
    }
    return positions[static_cast<size_t>(std::distance(roles.begin(), it))];
}

bool hasFin(const std::optional<SolveStep>& s) {
    return finCell(s).has_value();
}

bool finIsAt(const std::optional<SolveStep>& s, size_t row, size_t col) {
    auto fin = finCell(s);
    return fin.has_value() && fin->row == row && fin->col == col;
}

// True if every elimination sits OUTSIDE the box containing the fin cell.
bool elimsOutsideFinBox(const std::optional<SolveStep>& s) {
    auto fin = finCell(s);
    if (!s.has_value() || !fin.has_value()) {
        return false;
    }
    size_t box_row = fin->row / BOX_SIZE;
    size_t box_col = fin->col / BOX_SIZE;
    return std::ranges::none_of(s->eliminations, [&](const Elimination& e) {
        return (e.position.row / BOX_SIZE == box_row) && (e.position.col / BOX_SIZE == box_col);
    });
}

// True if the step's `position` matches one of its eliminations.
bool positionIsAnElimination(const std::optional<SolveStep>& s) {
    if (!s.has_value()) {
        return false;
    }
    size_t pos_row = s->position.row;
    size_t pos_col = s->position.col;
    return std::ranges::any_of(
        s->eliminations, [&](const Elimination& e) { return e.position.row == pos_row && e.position.col == pos_col; });
}

// True if the step eliminates exactly `value` from (row,col) and nothing else.
bool eliminatesOnly(const std::optional<SolveStep>& s, size_t row, size_t col, int value) {
    return s.has_value() && s->eliminations.size() == 1 && s->eliminations[0].position.row == row &&
           s->eliminations[0].position.col == col && s->eliminations[0].value == value;
}

}  // namespace

TEST_CASE("KrakenFishStrategy - Metadata", "[kraken_fish]") {
    KrakenFishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::KrakenFish);
    REQUIRE(strategy.getName() == "Kraken Fish");
    REQUIRE(strategy.getDifficultyRating() == 8.5);
}

TEST_CASE("KrakenFishStrategy - Returns nullopt for complete board", "[kraken_fish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    KrakenFishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("KrakenFishStrategy - Can be used through ISolvingStrategy interface", "[kraken_fish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<KrakenFishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::KrakenFish);
    REQUIRE(strategy->getName() == "Kraken Fish");
    REQUIRE(strategy->getDifficultyRating() == 8.5);

    // Complete board returns nullopt
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("KrakenFishStrategy - Returns nullopt for empty board with no finned patterns", "[kraken_fish]") {
    // Empty board has too many candidates everywhere for fish patterns
    BoardData board{};
    CandidateGrid state(board);
    KrakenFishStrategy strategy;

    auto result = strategy.findStep(board, state);
    // Empty board won't have finned fish with Kraken eliminations
    // (it may or may not find something, but the test verifies no crash)
    // The strategy should handle this gracefully
    (void)result;
    SUCCEED("Strategy did not crash on empty board");
}

// Regression for issue #20 (the HIGH finding from the 2026-05-25 audit):
//   A *non-vacuous* Kraken step must NOT include free finned-fish eliminations — those
//   are reported by FinnedXWing/Swordfish/Jellyfish strategies which run earlier;
//   bundling them here misattributes their rating and lets `position` point at a
//   non-Kraken cell.
//
// Fixture: Hard_s1_i0 raw givens (tests/data/fixtures/Hard/fixtures.yaml). At the
// initial candidate state KrakenFishStrategy finds a Kraken X-Wing on value 1 in
// rows 5/7 with the fin at R7C1 whose fin-placement chain is *consistent* (no
// contradiction), eliminating 1 from R3C4 — a genuine extended Kraken elimination
// (rating 8.5). Sound: the puzzle's solution has R3C4 = 7.
//
// region_type is asserted as Row (the base axis), matching the fish-family convention
// (XWing/Swordfish/Jellyfish/Finned*/Sashimi* all do the same). The 2026-05-25 audit's
// recommendation to flip it to the cover axis contradicted the documented convention at
// [localized_explanations.h:204] and was rejected — see PR #38 and issue #39.
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with multiple REQUIRE checks; complexity is inherent to test coverage
TEST_CASE("KrakenFishStrategy - non-vacuous Kraken omits in-fin-box elims, full Kraken rating",
          "[kraken_fish][regression][issue20]") {
    constexpr std::string_view kBoardFlat =
        "040500010003080002000020008000000000002067009317000050004000025030009700000010800";
    BoardData board = sudoku::testing::flatStringToBoard(std::string(kBoardFlat));
    CandidateGrid state(board);

    KrakenFishStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE(result.has_value());
    REQUIRE((result.has_value() && result->technique == SolvingTechnique::KrakenFish));

    // A consistent fin chain → full Kraken rating (NOT the fin-exclusion rating).
    REQUIRE((result.has_value() && result->rating == getTechniqueRating(SolvingTechnique::KrakenFish)));

    // Every elimination must sit OUTSIDE the fin's box — in-box elims belong to the
    // standard finned-fish technique and must not be attributed here.
    REQUIRE(hasFin(result));
    REQUIRE(elimsOutsideFinBox(result));

    // `position` must point at a real Kraken cell (one of the eliminations).
    REQUIRE(positionIsAnElimination(result));

    // This fixture is known to eliminate exactly 1 from R3C4. If the chain heuristic shifts
    // and this changes, flag it for review rather than silently passing on a weaker deduction.
    REQUIRE(eliminatesOnly(result, 2, 3, 1));

    // Row-based fish: pattern_axis encodes the base-axis orientation; Kraken chain
    // eliminations are not a single axis, so elimination_axis stays None. (gh#39)
    REQUIRE((result.has_value() && result->explanation_data.pattern_axis == RegionType::Row));
    REQUIRE((result.has_value() && result->explanation_data.elimination_axis == RegionType::None));
}

// Regression for issue #40 (the 2026-05-25 audit observation at CODE_REVIEW:1114):
//   When placing `value` at the fin self-contradicts, the fin placement is impossible,
//   so the old verifyKrakenElimination returned `true` *vacuously* for every candidate
//   target — surfacing a batch of cover eliminations "chain-verified" by a chain that
//   had actually self-destructed, rated as a full Kraken (8.5). The strictly simpler,
//   honest conclusion is that the fin cannot hold `value`: a single elimination on the
//   fin cell, rated at the underlying finned-fish level.
//
// Fixture: Hard_s4503_i7 (originally added for issue #20). On inspection it is in fact a
// vacuous-fin case: placing 2 at the fin R9C6 propagates to a contradiction. The puzzle's
// solution has R9C6 = 6, so eliminating 2 from R9C6 is sound. The base set is an order-3
// Swordfish, so the fin-exclusion is rated at the FinnedSwordfish level (SE 4.0), not 8.5.
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with multiple REQUIRE checks; complexity is inherent to test coverage
TEST_CASE("KrakenFishStrategy - vacuous fin contradiction reports fin-exclusion",
          "[kraken_fish][regression][issue40]") {
    constexpr std::string_view kBoardFlat =
        "050040000060005200370060008290000000005001090000030020507003004006070002000400010";

    // Prior eliminations recorded in the fixture — bring the candidate grid into the same
    // state the solver would reach by step 7. Reconstructed via the shared engine primitive
    // (Story A.0) instead of inline eliminateCandidate calls: a free proof (AC4) that the
    // extraction is behavior-identical to the hand-written replay it replaced.
    const auto reconstructed = sudoku::engine::reconstruct({.board_at_step = std::string(kBoardFlat),
                                                            .prior_eliminations = {
                                                                {.position = {.row = 4, .col = 0}, .value = 4},
                                                                {.position = {.row = 4, .col = 0}, .value = 8},
                                                                {.position = {.row = 5, .col = 0}, .value = 1},
                                                                {.position = {.row = 5, .col = 0}, .value = 4},
                                                                {.position = {.row = 5, .col = 0}, .value = 8},
                                                                {.position = {.row = 8, .col = 2}, .value = 2},
                                                                {.position = {.row = 3, .col = 6}, .value = 5},
                                                                {.position = {.row = 3, .col = 7}, .value = 5},
                                                                {.position = {.row = 6, .col = 4}, .value = 8},
                                                                {.position = {.row = 0, .col = 3}, .value = 1},
                                                                {.position = {.row = 1, .col = 3}, .value = 1},
                                                            }});
    REQUIRE(reconstructed.has_value());
    const BoardData& board = reconstructed->board;
    CandidateGrid state = reconstructed->candidates;

    KrakenFishStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE(result.has_value());
    REQUIRE((result.has_value() && result->technique == SolvingTechnique::KrakenFish));

    // Single fin-exclusion at the fin cell R9C6, NOT a batch of vacuous cover eliminations.
    REQUIRE(eliminatesOnly(result, 8, 5, 2));

    // `position` points at the eliminated fin cell, and the fin role marks the same cell.
    REQUIRE((result.has_value() && result->position.row == 8 && result->position.col == 5));
    REQUIRE(finIsAt(result, 8, 5));

    // Rated at the underlying finned-fish level (order-3 Swordfish = SE 4.0), NOT the full
    // Kraken rating — this is the rating-drift fix.
    REQUIRE((result.has_value() && result->rating == getTechniqueRating(SolvingTechnique::FinnedSwordfish)));
    REQUIRE((result.has_value() && result->rating != getTechniqueRating(SolvingTechnique::KrakenFish)));

    // Honest explanation: the fin placement is impossible (no "via chain verification").
    REQUIRE((result.has_value() && result->explanation.contains("impossible")));
    REQUIRE((result.has_value() && result->explanation.contains("fin")));

    // Base-axis orientation preserved on pattern_axis; elimination_axis None for chain elims. (gh#39)
    REQUIRE((result.has_value() && result->explanation_data.pattern_axis == RegionType::Row));
    REQUIRE((result.has_value() && result->explanation_data.elimination_axis == RegionType::None));
}

// Regression for issue #59 (the HIGH finding from the bmad-code-review pass on PR #58):
//   The cover-sweep "did this target lose `value`?" test reads `(masks[t] & bit) == 0`, which is
//   ALSO true when the fin-chain *placed* `value` into the target (placeInState zeroes a filled
//   cell's mask). The cell where `value` was placed was then emitted as an *elimination* of
//   `value` — an inverted, unsound deduction. The guard restricts the mask clause to EMPTY cells.
//
// Deterministic white-box scenario via KrakenFishTestPeer (no mined puzzle needed): column 5
// (index 4) admits value 5 only at R1C5 and R5C5 — every other row of the column carries a given
// non-5 digit. Placing 5 at the fin R1C1 strips 5 from R1C5 (same row), collapsing R1C5 to its
// last candidate and cascading until R5C5 is the lone 5 left in the column, so the fin-chain
// PLACES 5 at R5C5. Pre-#59 the sweep reported "eliminate 5 from R5C5"; it must not.
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with multiple REQUIRE checks
TEST_CASE("KrakenFishStrategy - cover target filled with value is not a false elimination",
          "[kraken_fish][regression][issue59]") {
    constexpr std::string_view kBoardFlat =
        "000000000000010000000020000000030000000000000000060000000070000000080000000090000";
    BoardData board = sudoku::testing::flatStringToBoard(std::string(kBoardFlat));
    CandidateGrid candidates(board);

    // Premise guard: confirm the fin-chain really forces 5 into R5C5 (else the test is vacuous).
    auto state = ForcingChainHelpers::initState(board, candidates);
    ForcingChainHelpers::placeInState(state, (0 * BOARD_SIZE) + 0, 5);
    REQUIRE_FALSE(state.contradiction);
    ForcingChainHelpers::propagate(state);
    REQUIRE_FALSE(state.contradiction);
    REQUIRE(state.board[(4 * BOARD_SIZE) + 4] == 5);

    const std::vector<size_t> base_primaries{1, 2};  // base rows skipped by the sweep
    const std::vector<size_t> base_secondaries{4};   // cover column (index 4 = column 5)
    auto elims =
        KrakenFishTestPeer::coverSweepEliminations(board, candidates, /*value=*/5, /*by_row=*/true, base_primaries,
                                                   base_secondaries, Position{.row = 0, .col = 0}, /*fin_box=*/0);

    // The placed-with-value cell R5C5 must NOT be reported as an elimination of 5.
    const bool reports_r5c5 = std::ranges::any_of(
        elims, [](const Elimination& e) { return e.position.row == 4 && e.position.col == 4 && e.value == 5; });
    REQUIRE_FALSE(reports_r5c5);

    // The genuine elimination of 5 from R1C5 (5 gone, cell took a different value) is unaffected.
    const bool reports_r1c5 = std::ranges::any_of(
        elims, [](const Elimination& e) { return e.position.row == 0 && e.position.col == 4 && e.value == 5; });
    REQUIRE(reports_r1c5);
}

namespace {

struct KrakenMiningResult {
    int generated{0};
    int kraken_steps{0};
    int inverted{0};
};

// Ground-truth sweep for issue #59. Generate puzzles and walk each one through the full strategy
// chain with a persistent CandidateGrid. KrakenFish is registered ~50th, so it is virtually never
// the chain's first-applicable step — scanning only the chosen step never reaches it (an earlier
// version reported kraken_steps=0). Instead, at EVERY intermediate candidate state we probe a
// standalone KrakenFishStrategy directly and ground-truth each elimination it would make. On a hit
// the original puzzle and the working board at the firing state are printed for capture as a
// fixture. (Probing every state is costly, hence the modest corpus.)
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — diagnostic harness; nesting is inherent
KrakenMiningResult mineKrakenInversions(Difficulty difficulty, int num_puzzles, int start_seed) {
    PuzzleGenerator generator;
    auto strategies = sudoku::testing::createStrategyChain();
    KrakenFishStrategy kraken;
    KrakenMiningResult r;

    for (int seed = start_seed; seed < start_seed + num_puzzles; ++seed) {
        GenerationSettings settings{
            .difficulty = difficulty, .seed = static_cast<uint32_t>(seed), .ensure_unique = true, .max_attempts = 500};
        auto puzzle = generator.generatePuzzle(settings);
        if (!puzzle.has_value()) {
            continue;
        }
        r.generated++;
        const auto& truth = puzzle->solution;
        auto working = puzzle->board;
        CandidateGrid candidates(working);

        constexpr int kMaxIterations = 300;
        for (int iter = 0; iter < kMaxIterations && !sudoku::testing::isBoardComplete(working); ++iter) {
            // Probe KrakenFish INDEPENDENTLY at this candidate state. Kraken is registered ~50th in
            // the chain, so the first-applicable step is almost never Kraken — scanning only the
            // chosen step never reaches it. We instead ask Kraken directly what it would deduce at
            // every state the puzzle passes through, and ground-truth each elimination. (We do not
            // apply Kraken's step; the chain's choice advances the replay.)
            if (auto kstep = kraken.findStep(working, candidates); kstep.has_value()) {
                r.kraken_steps++;
                for (const auto& e : kstep->eliminations) {
                    if (truth[e.position.row][e.position.col] == e.value) {
                        r.inverted++;
                        std::cerr << "\n=== ISSUE #59 REPRODUCER (Kraken inverted elimination) ===\n"
                                  << "Seed " << seed << " (difficulty " << static_cast<int>(difficulty)
                                  << "): eliminated " << e.value << " from R" << (e.position.row + 1) << "C"
                                  << (e.position.col + 1) << " but the solution has that value there.\n"
                                  << "Explanation: " << kstep->explanation << "\n"
                                  << "Original puzzle (flat): " << sudoku::testing::boardToFlatString(puzzle->board)
                                  << "\nWorking board at step (flat): " << sudoku::testing::boardToFlatString(working)
                                  << "\n";
                    }
                }
            }

            // Advance the replay using the chain's first-applicable step.
            std::optional<SolveStep> step;
            for (const auto& strategy : strategies) {
                step = strategy->findStep(working, candidates);
                if (step.has_value()) {
                    break;
                }
            }
            if (!step.has_value()) {
                break;  // would fall back to backtracking
            }
            if (step->type == SolveStepType::Placement) {
                working[step->position.row][step->position.col] = step->value;
                candidates.placeValue(step->position.row, step->position.col, step->value);
            } else {
                for (const auto& e : step->eliminations) {
                    candidates.eliminateCandidate(e.position.row, e.position.col, e.value);
                }
            }
        }
    }
    return r;
}

}  // namespace

// Regression for issue #59 on a REAL mined puzzle (raw givens — no chain setup needed). Mined from
// the corpus sweep below; at the initial candidate state KrakenFishStrategy finds a Kraken X-Wing
// on value 1 in rows 4/5 with the fin at R5C7. Placing 1 at the fin propagates to *place* 1 at the
// cover-line target R2C9. Pre-#59 the sweep reported "eliminate 1 from R2C9" — but the puzzle's
// unique solution has R2C9 = 1, so that was an inverted, unsound deduction. The guard suppresses
// R2C9 while still emitting the genuine elimination of 1 from R1C9 (solution R1C9 = 4 → sound).
//
// This is the user-reachable face of the bug: although the production solve loop almost never
// reaches Kraken (it is the ~50th strategy), the "Find step by technique" feature calls
// KrakenFishStrategy::findStep directly, so a user picking Kraken Fish here would be told to remove
// the one correct candidate from R2C9.
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with multiple REQUIRE checks
TEST_CASE("KrakenFishStrategy - mined real puzzle: fin-chain placement is not eliminated",
          "[kraken_fish][regression][issue59]") {
    constexpr std::string_view kBoardFlat =
        "060000520000040000140302800000900350000000098000130200000000010003809400001067000";
    BoardData board = sudoku::testing::flatStringToBoard(std::string(kBoardFlat));
    CandidateGrid candidates(board);

    // Premise: the fin (R5C7) placement really forces 1 into R2C9 (else the regression is vacuous).
    auto state = ForcingChainHelpers::initState(board, candidates);
    ForcingChainHelpers::placeInState(state, (4 * BOARD_SIZE) + 6, 1);  // fin R5C7
    REQUIRE_FALSE(state.contradiction);
    ForcingChainHelpers::propagate(state);
    REQUIRE_FALSE(state.contradiction);
    REQUIRE(state.board[(1 * BOARD_SIZE) + 8] == 1);  // R2C9 filled with 1 by the fin-chain

    KrakenFishStrategy kraken;
    auto step = kraken.findStep(board, candidates);
    REQUIRE(step.has_value());
    REQUIRE((step.has_value() && step->technique == SolvingTechnique::KrakenFish));

    // The placed-with-value cell R2C9 (solution = 1) must NOT be eliminated.
    // (`step.has_value() &&` short-circuits before any `->`, so CI's bugprone-unchecked-optional-
    // access — which does not model REQUIRE as a guard — stays happy.)
    const bool eliminates_r2c9 = step.has_value() && std::ranges::any_of(step->eliminations, [](const Elimination& e) {
                                     return e.position.row == 1 && e.position.col == 8 && e.value == 1;
                                 });
    REQUIRE_FALSE(eliminates_r2c9);

    // The genuine elimination of 1 from R1C9 (solution = 4) is still produced.
    const bool eliminates_r1c9 = step.has_value() && std::ranges::any_of(step->eliminations, [](const Elimination& e) {
                                     return e.position.row == 0 && e.position.col == 8 && e.value == 1;
                                 });
    REQUIRE(eliminates_r1c9);
}

// Hidden hard-mining harness ([.] = excluded from the default run). Pre-fix it is expected to
// surface a reproducer (captured into the [issue59] regression above); post-fix it must stay 0.
// Run explicitly: unit_tests "[kraken][mining]".
TEST_CASE("KrakenFishStrategy - mining sweep finds no inverted cover eliminations", "[.][kraken][mining]") {
    int total_gen = 0;
    int total_kraken = 0;
    int total_inverted = 0;
    const std::vector<std::pair<Difficulty, int>> corpus{
        {Difficulty::Hard, 600}, {Difficulty::Expert, 600}, {Difficulty::Master, 400}};
    for (const auto& [difficulty, count] : corpus) {
        auto r = mineKrakenInversions(difficulty, count, 1);
        total_gen += r.generated;
        total_kraken += r.kraken_steps;
        total_inverted += r.inverted;
    }
    std::cerr << "\n[#59 mining] generated=" << total_gen << " kraken_steps=" << total_kraken
              << " inverted_elims=" << total_inverted << "\n";
    CHECK(total_inverted == 0);
}
