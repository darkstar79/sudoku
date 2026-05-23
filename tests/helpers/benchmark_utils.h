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
 * @file benchmark_utils.h
 * @brief Shared utilities for puzzle generation benchmarks
 *
 * Provides configurable benchmark infrastructure with YAML-based configuration,
 * statistical analysis, and reusable data structures.
 *
 * Usage:
 *   BenchmarkConfig config = BenchmarkConfig::fromYAML("configs/standard.yaml").value();
 *   BenchmarkResult result = runBenchmark(generator, config);
 *   result.printSummary(std::cout);
 */

#pragma once

#include "core/i_puzzle_generator.h"

#include <chrono>
#include <expected>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace sudoku::testing {

/// Configuration for benchmark runs (loaded from YAML)
struct BenchmarkConfig {
    core::Difficulty difficulty;
    std::string difficulty_name;  // "Easy", "Medium", "Hard", "Expert"
    int iterations{20};           // How many puzzles to generate
    int max_attempts{1000};       // Max solver attempts per puzzle (not currently used)
    std::vector<uint32_t> seeds;  // RNG seeds (empty = random)
    bool verbose{false};          // Print per-iteration output

    /// Load from YAML file
    [[nodiscard]] static std::expected<BenchmarkConfig, std::string> fromYAML(const std::string& filepath);
};

/// Results from benchmark run (extracted from benchmark_phase2.cpp)
struct BenchmarkResult {
    std::string difficulty;
    int attempts;
    int successes;
    int failures;
    double success_rate;
    double avg_time_ms;
    double median_time_ms;
    double min_time_ms;
    double max_time_ms;
    double avg_clues;

    /// Print formatted summary
    void printSummary(std::ostream& os) const;
};

/// Helper: Count non-zero cells in board (clue count)
[[nodiscard]] int countClues(const core::BoardData& board);

/// Run puzzle generation benchmark with given config
/// Template allows working with any IPuzzleGenerator implementation
template <typename Generator>
[[nodiscard]] BenchmarkResult runBenchmark(Generator& generator, const BenchmarkConfig& config);

}  // namespace sudoku::testing

// Template implementation (must be in header)
#include "benchmark_utils_impl.h"
