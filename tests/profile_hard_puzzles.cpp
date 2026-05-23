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
 * Statistical profiling program for Hard puzzle generation.
 *
 * Generates 50 Hard puzzles with different seeds to measure:
 * - Mean performance
 * - Standard deviation
 * - Min/max execution times
 * - Success rate
 *
 * Usage:
 *   # Native execution for wall-clock time
 *   ./profile_hard_puzzles
 *
 *   # Valgrind profiling for instruction counts
 *   valgrind --tool=callgrind ./profile_hard_puzzles
 */

#include "../src/core/puzzle_generator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

using namespace sudoku::core;

struct BenchmarkResult {
    int seed;
    bool success;
    int clue_count;
    long long duration_ms;
};

void printStatistics(const std::vector<BenchmarkResult>& results) {
    std::vector<long long> durations;
    int successes = 0;

    for (const auto& result : results) {
        if (result.success) {
            durations.push_back(result.duration_ms);
            successes++;
        }
    }

    if (durations.empty()) {
        std::cout << "No successful puzzle generations!\n";
        return;
    }

    // Calculate statistics
    long long sum = 0;
    for (long long duration : durations) {
        sum += duration;
    }
    double mean = static_cast<double>(sum) / static_cast<double>(durations.size());

    double variance = 0.0;
    for (long long duration : durations) {
        double diff = static_cast<double>(duration) - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(durations.size());
    double std_dev = std::sqrt(variance);

    auto min_duration = *std::min_element(durations.begin(), durations.end());
    auto max_duration = *std::max_element(durations.begin(), durations.end());

    std::cout << "\n=== STATISTICS (n=" << successes << ") ===\n";
    std::cout << "Success rate: " << successes << "/" << results.size() << " ("
              << (100.0 * static_cast<double>(successes) / static_cast<double>(results.size())) << "%)\n";
    std::cout << "Mean:         " << mean << " ms\n";
    std::cout << "Std Dev:      " << std_dev << " ms\n";
    std::cout << "Min:          " << min_duration << " ms\n";
    std::cout << "Max:          " << max_duration << " ms\n";
}

int main() {
    PuzzleGenerator generator;
    std::vector<BenchmarkResult> results;

    std::cout << "=== Hard Puzzle Benchmark (50 puzzles) ===\n";
    std::cout << "This may take 10-30 seconds...\n";
    std::cout << "\n";

    const int NUM_PUZZLES = 50;
    const int BASE_SEED = 10000;
    const int MAX_ATTEMPTS = 200;

    for (int i = 0; i < NUM_PUZZLES; ++i) {
        int seed = BASE_SEED + (i * 1000);

        GenerationSettings settings;
        settings.difficulty = Difficulty::Hard;
        settings.seed = seed;
        settings.ensure_unique = true;
        settings.max_attempts = MAX_ATTEMPTS;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        BenchmarkResult benchmark{.seed = seed,
                                  .success = result.has_value(),
                                  .clue_count = result.has_value() ? result->clue_count : 0,
                                  .duration_ms = duration.count()};

        if (result.has_value()) {
            std::cout << "[" << (i + 1) << "/" << NUM_PUZZLES << "] "
                      << "Seed " << seed << ": " << duration.count() << "ms, " << result->clue_count << " clues\n";
        } else {
            std::cout << "[" << (i + 1) << "/" << NUM_PUZZLES << "] "
                      << "Seed " << seed << ": FAILED after " << MAX_ATTEMPTS << " attempts\n";
        }

        results.push_back(benchmark);
    }

    printStatistics(results);

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
