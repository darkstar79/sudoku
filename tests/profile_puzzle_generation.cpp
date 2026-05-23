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

/**
 * Standalone profiling program for puzzle generation, uniqueness checking, and rating.
 *
 * Exercises three stages independently so callgrind can attribute costs to each:
 * 1. Puzzle generation (solution + clue removal + interleaved uniqueness checks)
 * 2. Standalone uniqueness check (single hasUniqueSolution call)
 * 3. Puzzle rating (strategy-based difficulty scoring)
 *
 * Usage:
 *   # Native execution for wall-clock time
 *   ./profile_puzzle_generation
 *
 *   # Valgrind profiling for instruction counts
 *   valgrind --tool=callgrind ./profile_puzzle_generation
 *   callgrind_annotate --auto=yes callgrind.out.<pid>
 *   kcachegrind callgrind.out.<pid>
 */

#include "../src/core/game_validator.h"
#include "../src/core/puzzle_generator.h"
#include "../src/core/puzzle_rater.h"
#include "../src/core/puzzle_rating.h"
#include "../src/core/sudoku_solver.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

using namespace sudoku::core;

static std::string difficultyName(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return "Easy";
        case Difficulty::Medium:
            return "Medium";
        case Difficulty::Hard:
            return "Hard";
        case Difficulty::Expert:
            return "Expert";
        case Difficulty::Master:
            return "Master";
    }
    return "Unknown";
}

static void profilePuzzle(PuzzleGenerator& generator, PuzzleRater& rater, Difficulty difficulty, uint32_t seed,
                          int max_attempts) {
    std::cout << "\n--- " << difficultyName(difficulty) << " (seed " << seed << ") ---\n";

    GenerationSettings settings;
    settings.difficulty = difficulty;
    settings.seed = seed;
    settings.ensure_unique = true;
    settings.max_attempts = max_attempts;

    // Stage 1: Puzzle generation
    auto gen_start = std::chrono::high_resolution_clock::now();
    auto result = generator.generatePuzzle(settings);
    auto gen_end = std::chrono::high_resolution_clock::now();
    auto gen_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();

    if (!result.has_value()) {
        std::cout << "  Generation: FAILED after " << max_attempts << " attempts (" << std::fixed
                  << std::setprecision(1) << gen_ms << "ms)\n";
        return;
    }

    std::cout << "  Generation:      " << std::fixed << std::setprecision(1) << gen_ms << "ms (" << result->clue_count
              << " clues)\n";

    // Stage 2: Standalone uniqueness check
    auto uniq_start = std::chrono::high_resolution_clock::now();
    bool unique = generator.hasUniqueSolution(result->board);
    auto uniq_end = std::chrono::high_resolution_clock::now();
    auto uniq_ms = std::chrono::duration<double, std::milli>(uniq_end - uniq_start).count();

    std::cout << "  Uniqueness:      " << std::fixed << std::setprecision(1) << uniq_ms << "ms ("
              << (unique ? "unique" : "NOT unique") << ")\n";

    // Stage 3: Puzzle rating
    auto rate_start = std::chrono::high_resolution_clock::now();
    auto rating = rater.ratePuzzle(result->board);
    auto rate_end = std::chrono::high_resolution_clock::now();
    auto rate_ms = std::chrono::duration<double, std::milli>(rate_end - rate_start).count();

    if (rating.has_value()) {
        std::cout << "  Rating:          " << std::fixed << std::setprecision(1) << rate_ms << "ms (score "
                  << rating->se_rating << ", " << difficultyName(rating->estimated_difficulty)
                  << (rating->requires_backtracking ? ", backtracking" : "") << ")\n";
    } else {
        std::cout << "  Rating:          " << std::fixed << std::setprecision(1) << rate_ms << "ms (FAILED)\n";
    }

    double total_ms = gen_ms + uniq_ms + rate_ms;
    std::cout << "  Total:           " << std::fixed << std::setprecision(1) << total_ms << "ms"
              << "  [gen " << std::setprecision(0) << (gen_ms / total_ms * 100.0) << "% | uniq "
              << (uniq_ms / total_ms * 100.0) << "% | rate " << (rate_ms / total_ms * 100.0) << "%]\n";
}

int main() {
    // Set up dependencies
    auto validator = std::make_shared<GameValidator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);
    PuzzleGenerator generator;

    std::cout << "=== Puzzle Generation Profiling (3-stage, Hard only) ===\n";
    std::cout << "Stages: generation | uniqueness check | rating\n";
    std::cout << "Use with: valgrind --tool=callgrind ./profile_puzzle_generation\n";

    // Hard puzzles only — these dominate instruction counts and are the
    // meaningful profiling target. Multiple seeds for callgrind coverage.
    profilePuzzle(generator, rater, Difficulty::Hard, 10000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 20000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 40000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 50000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 60000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 70000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 80000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 90000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 11000, 500);
    profilePuzzle(generator, rater, Difficulty::Hard, 22000, 500);

    std::cout << "\nProfiling complete.\n";
    return 0;
}
