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
 * Throughput profiling for Expert puzzle generation.
 *
 * Measures generation throughput (attempts per second) for a single Expert
 * puzzle over a 60-second time window. This tests whether memoization
 * provides measurable benefit for Expert difficulty (17-21 clues) which
 * has more backtracking and potentially higher cache hit rates.
 *
 * Metrics measured:
 * - Total attempts completed in 60 seconds
 * - Attempts per second (throughput)
 * - Mean time per attempt
 * - Best puzzle achieved (minimum clues)
 *
 * Usage:
 *   ./profile_expert_puzzles
 *
 * Compare across configurations to test memoization effectiveness.
 */

#include "../src/core/puzzle_generator.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>

using namespace sudoku::core;

int main() {
    const int TIMEOUT_SEC = 60;
    const int SEED = 99999;  // Fixed seed for reproducibility across configs

    std::cout << "=== Expert Puzzle Throughput Benchmark ===\n";
    std::cout << "Duration: " << TIMEOUT_SEC << " seconds\n";
    std::cout << "Seed: " << SEED << "\n";
    std::cout << "Difficulty: Expert (17-21 clues)\n";
    std::cout << "\n";
    std::cout << "Starting benchmark (this will take " << TIMEOUT_SEC << " seconds)...\n";
    std::cout << std::flush;

    PuzzleGenerator generator;
    auto benchmark_start = std::chrono::steady_clock::now();

    int total_attempts = 0;
    int successful_attempts = 0;
    int min_clues_achieved = 81;  // Maximum possible
    std::vector<long long> attempt_durations;

    // Run for 60 seconds, measuring throughput
    while (true) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - benchmark_start);

        if (elapsed.count() >= TIMEOUT_SEC) {
            break;
        }

        // Generate single attempt
        GenerationSettings settings;
        settings.difficulty = Difficulty::Expert;
        settings.seed = SEED;
        settings.ensure_unique = true;
        settings.max_attempts = 1;  // Single attempt per loop iteration

        auto attempt_start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto attempt_end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(attempt_end - attempt_start);

        attempt_durations.push_back(duration.count());
        total_attempts++;

        if (result.has_value()) {
            successful_attempts++;
            min_clues_achieved = std::min(min_clues_achieved, result->clue_count);
        }

        // Progress indicator every 10 attempts
        if (total_attempts % 10 == 0) {
            std::cout << "." << std::flush;
        }
    }

    std::cout << "\n\n";

    // Calculate statistics
    double attempts_per_sec = static_cast<double>(total_attempts) / static_cast<double>(TIMEOUT_SEC);

    long long sum = std::accumulate(attempt_durations.begin(), attempt_durations.end(), 0LL);
    double mean_duration = static_cast<double>(sum) / static_cast<double>(total_attempts);

    auto min_duration = *std::min_element(attempt_durations.begin(), attempt_durations.end());
    auto max_duration = *std::max_element(attempt_durations.begin(), attempt_durations.end());

    // Calculate standard deviation
    double variance = 0.0;
    for (long long duration : attempt_durations) {
        double diff = static_cast<double>(duration) - mean_duration;
        variance += diff * diff;
    }
    variance /= static_cast<double>(total_attempts);
    double std_dev = std::sqrt(variance);

    // Print results
    std::cout << "=== THROUGHPUT RESULTS ===\n";
    std::cout << "Total attempts:      " << total_attempts << "\n";
    std::cout << "Successful attempts: " << successful_attempts << "\n";
    std::cout << "Success rate:        "
              << (100.0 * static_cast<double>(successful_attempts) / static_cast<double>(total_attempts)) << "%\n";
    std::cout << "\n";
    std::cout << "=== THROUGHPUT METRICS ===\n";
    std::cout << "Attempts/sec:        " << attempts_per_sec << "\n";
    std::cout << "Mean time/attempt:   " << mean_duration << " ms\n";
    std::cout << "Std Dev:             " << std_dev << " ms\n";
    std::cout << "Min time/attempt:    " << min_duration << " ms\n";
    std::cout << "Max time/attempt:    " << max_duration << " ms\n";
    std::cout << "\n";

    if (successful_attempts > 0) {
        std::cout << "=== BEST PUZZLE ACHIEVED ===\n";
        std::cout << "Minimum clues:       " << min_clues_achieved << " clues\n";
    } else {
        std::cout << "No successful puzzles generated in " << TIMEOUT_SEC << " seconds\n";
    }

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
