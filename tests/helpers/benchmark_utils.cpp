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

/**
 * @file benchmark_utils.cpp
 * @brief Implementation of benchmark utilities
 */

#include "benchmark_utils.h"

#include <fstream>
#include <iostream>

#include <fmt/format.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::testing {

int countClues(const core::BoardData& board) {
    int count = 0;
    for (const auto& row : board) {
        for (int cell : row) {
            if (cell != 0) {
                count++;
            }
        }
    }
    return count;
}

void BenchmarkResult::printSummary(std::ostream& os) const {
    os << "\n" << difficulty << " Difficulty:\n";
    os << "  Success Rate:  " << std::fixed << std::setprecision(1) << success_rate << "% (" << successes << "/"
       << attempts << ")\n";
    os << "  Avg Time:      " << std::fixed << std::setprecision(1) << avg_time_ms << "ms\n";
    os << "  Median Time:   " << std::fixed << std::setprecision(1) << median_time_ms << "ms\n";
    os << "  Range:         " << std::fixed << std::setprecision(1) << min_time_ms << "ms - " << max_time_ms << "ms\n";
    if (avg_clues > 0) {
        os << "  Avg Clues:     " << std::fixed << std::setprecision(1) << avg_clues << "\n";
    }
}

// Helper: Parse difficulty string to enum
static std::optional<core::Difficulty> parseDifficulty(const std::string& str) {
    if (str == "Easy") {
        return core::Difficulty::Easy;
    }
    if (str == "Medium") {
        return core::Difficulty::Medium;
    }
    if (str == "Hard") {
        return core::Difficulty::Hard;
    }
    if (str == "Expert") {
        return core::Difficulty::Expert;
    }
    if (str == "Master") {
        return core::Difficulty::Master;
    }
    return std::nullopt;
}

std::expected<BenchmarkConfig, std::string> BenchmarkConfig::fromYAML(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);

        if (!root["benchmarks"]) {
            return std::unexpected("Missing 'benchmarks' key in YAML config");
        }

        // For now, load first benchmark config
        // Future: support multiple configs in single file
        const auto& bench = root["benchmarks"][0];

        if (!bench["difficulty"]) {
            return std::unexpected("Missing 'difficulty' field in benchmark config");
        }

        std::string diff_str = bench["difficulty"].as<std::string>();
        auto difficulty = parseDifficulty(diff_str);
        if (!difficulty.has_value()) {
            return std::unexpected(
                fmt::format("Invalid difficulty: {} (expected: Easy, Medium, Hard, Expert)", diff_str));
        }

        BenchmarkConfig config;
        config.difficulty = *difficulty;
        config.difficulty_name = diff_str;

        // Optional fields with defaults
        if (bench["iterations"]) {
            config.iterations = bench["iterations"].as<int>();
        }

        if (bench["max_attempts"]) {
            config.max_attempts = bench["max_attempts"].as<int>();
        }

        if (bench["verbose"]) {
            config.verbose = bench["verbose"].as<bool>();
        }

        if (bench["seeds"]) {
            const auto& seeds_node = bench["seeds"];
            if (seeds_node.IsSequence()) {
                for (const auto& seed : seeds_node) {
                    config.seeds.push_back(seed.as<uint32_t>());
                }
            }
        }

        return config;

    } catch (const YAML::Exception& e) {
        return std::unexpected(fmt::format("YAML parsing error: {}", e.what()));
    } catch (const std::exception& e) {
        return std::unexpected(fmt::format("Error loading config file: {}", e.what()));
    }
}

}  // namespace sudoku::testing
