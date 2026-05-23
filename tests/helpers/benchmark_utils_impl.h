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
 * @file benchmark_utils_impl.h
 * @brief Template implementation for benchmark utilities
 *
 * Contains runBenchmark() template function that must be in header.
 * Automatically included by benchmark_utils.h - do not include directly.
 */

#pragma once

#include "benchmark_utils.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace sudoku::testing {

template <typename Generator>
[[nodiscard]] BenchmarkResult runBenchmark(Generator& generator, const BenchmarkConfig& config) {
    std::vector<double> times;
    std::vector<int> clue_counts;
    int successes = 0;
    int failures = 0;

    if (config.verbose) {
        std::cout << "\nBenchmarking " << config.difficulty_name << " difficulty (" << config.iterations
                  << " iterations)...\n";
    }

    for (int i = 0; i < config.iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(config.difficulty);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        double time_ms = static_cast<double>(duration.count());
        times.push_back(time_ms);

        if (result.has_value()) {
            const auto& puzzle = result.value();
            int clues = countClues(puzzle.board);
            clue_counts.push_back(clues);
            successes++;

            if (config.verbose) {
                std::cout << "  [" << std::setw(2) << (i + 1) << "/" << config.iterations << "] ✓ " << std::fixed
                          << std::setprecision(1) << time_ms << "ms (" << clues << " clues)\n";
            }
        } else {
            failures++;

            if (config.verbose) {
                std::cout << "  [" << std::setw(2) << (i + 1) << "/" << config.iterations << "] ✗ " << std::fixed
                          << std::setprecision(1) << time_ms << "ms (failed)\n";
            }
        }
    }

    // Calculate statistics
    double success_rate = (static_cast<double>(successes) / config.iterations) * 100.0;
    double avg_time = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    std::vector<double> sorted_times = times;
    std::sort(sorted_times.begin(), sorted_times.end());
    double median_time = sorted_times[sorted_times.size() / 2];
    double min_time = *std::min_element(times.begin(), times.end());
    double max_time = *std::max_element(times.begin(), times.end());

    double avg_clues =
        clue_counts.empty() ? 0.0 : std::accumulate(clue_counts.begin(), clue_counts.end(), 0.0) / clue_counts.size();

    return BenchmarkResult{config.difficulty_name,
                           config.iterations,
                           successes,
                           failures,
                           success_rate,
                           avg_time,
                           median_time,
                           min_time,
                           max_time,
                           avg_clues};
}

}  // namespace sudoku::testing
