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

// Class-C bucket-migration histogram — Story 0b.4c DoD gate.
//
// Class C makes a larger fish / longer chain rate higher than a smaller/shorter one of the same technique.
// That can in principle push a puzzle across a difficulty-bucket boundary (3.8 / 5.5 / 7.5), so this gate
// quantifies HOW MANY generated puzzles change bucket and asserts the structural invariants that keep the
// shift bounded:
//   1. scaling never LOWERS a rating (the term is non-negative),
//   2. a puzzle's bucket can only move UP, never down, and
//   3. the migrated fraction stays under the "no unexpected large shift" threshold (Task 0) — the gate that
//      would catch a coefficient silently re-banding a whole region of the Hard/Expert distribution.
//
// The OLD flat (pre-0b.4c) rating is recovered from the SAME solve path as the live scaled rating, so no
// second code version is needed: per step, the flat getTechniqueRating(technique) is the old value and the
// step's own (context-scaled) .rating is the new value; a puzzle's rating is the max over its steps.
//
// Heavy + generation-bound (difficulty-validated generation across Hard/Expert/Master), so it is [.]-hidden —
// run manually on Release/RWDI for the DoD gate, exactly like [strategy][correctness]. The from→to histogram
// is printed to stdout so the numbers can be lifted into the CHANGELOG / commit body.

#include "../../src/core/game_validator.h"
#include "../../src/core/i_puzzle_generator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/puzzle_rater.h"
#include "../../src/core/puzzle_rating.h"
#include "../../src/core/solve_step.h"
#include "../../src/core/solving_technique.h"
#include "../../src/core/sudoku_solver.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

struct RatingPair {
    double old_flat{0.0};    ///< max over steps of the flat (pre-0b.4c) getTechniqueRating(technique)
    double new_scaled{0.0};  ///< max over steps of the live context-scaled step.rating
};

[[nodiscard]] RatingPair maxRatings(const std::vector<SolveStep>& path) {
    RatingPair r;
    for (const auto& step : path) {
        if (step.technique == SolvingTechnique::Backtracking) {
            continue;
        }
        r.old_flat = std::max(r.old_flat, getTechniqueRating(step.technique));
        r.new_scaled = std::max(r.new_scaled, step.rating);
    }
    return r;
}

struct CorpusBand {
    Difficulty difficulty;
    int count;
};

// Fixed-seed corpus (deterministic): mirrors the 0b-4c review profiler's seed stride. Hard/Expert carry the
// in-scope chain families; Master is sampled lightly (Class-C is empirically inert there — its argmax steps
// are all out-of-scope ALS/forcing-chain techniques).
constexpr std::array<CorpusBand, 3> kCorpus = {{
    {.difficulty = Difficulty::Hard, .count = 60},
    {.difficulty = Difficulty::Expert, .count = 60},
    {.difficulty = Difficulty::Master, .count = 20},
}};
constexpr std::uint32_t kSeedBase = 20000;
constexpr std::uint32_t kSeedStride = 101;
constexpr int kGenerationAttempts = 300;

// Task 0 threshold: at most this fraction of the corpus may change bucket. The ceiling is closed by
// construction for the only material family (XYChain 6.6, length-bounded → 7.1 < 7.5), so the expected
// migration count is ~0; 2% leaves headroom for a rare long ContinuousNiceLoop without being vacuous.
constexpr double kMaxBucketShiftFraction = 0.02;

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("Class-C scaling — bucket-migration histogram stays within threshold", "[rating][bucket_migration][.]") {
    auto validator = std::make_shared<GameValidator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto rater = std::make_shared<PuzzleRater>(solver);
    // Full chain (generator WITH rater) → puzzles are difficulty-validated; the default ctor would skip rating.
    auto generator = std::make_shared<PuzzleGenerator>(rater);

    int total = 0;
    int migrated = 0;
    std::map<std::pair<int, int>, int> histogram;  // (old_bucket, new_bucket) → count

    for (const auto& band : kCorpus) {
        for (int i = 0; i < band.count; ++i) {
            GenerationSettings settings;
            settings.difficulty = band.difficulty;
            settings.seed = kSeedBase + (static_cast<std::uint32_t>(i) * kSeedStride);
            settings.ensure_unique = true;
            settings.max_attempts = kGenerationAttempts;

            auto puzzle = generator->generatePuzzle(settings);
            if (!puzzle.has_value()) {
                continue;  // generation may miss the band within max_attempts — skip, do not fail the gate
            }
            auto rating = rater->ratePuzzle(puzzle->board);
            if (!rating.has_value()) {
                continue;
            }

            const RatingPair r = maxRatings(rating->solve_path);

            // Invariant 1: structural scaling never lowers a rating.
            REQUIRE(r.new_scaled >= r.old_flat);

            const Difficulty old_bucket = ratingToDifficulty(r.old_flat);
            const Difficulty new_bucket = ratingToDifficulty(r.new_scaled);
            ++total;
            if (old_bucket != new_bucket) {
                ++migrated;
                ++histogram[{static_cast<int>(old_bucket), static_cast<int>(new_bucket)}];
                // Invariant 2: buckets only move UP (scaling is non-decreasing).
                REQUIRE(static_cast<int>(new_bucket) > static_cast<int>(old_bucket));
            }
        }
    }

    REQUIRE(total > 0);

    // Always-on histogram dump (for the CHANGELOG / commit body). Difficulty ints: 2=Hard 3=Expert 4=Master.
    std::cout << "[0b-4c bucket-migration] migrated " << migrated << " / " << total << " puzzles\n";
    for (const auto& [transition, count] : histogram) {
        std::cout << "  bucket " << transition.first << " -> " << transition.second << " : " << count << " puzzle(s)\n";
    }

    // Invariant 3: no unexpected large shift — the gate that catches a region-re-banding coefficient.
    const double migrated_fraction = static_cast<double>(migrated) / static_cast<double>(total);
    REQUIRE(migrated_fraction <= kMaxBucketShiftFraction);
}
