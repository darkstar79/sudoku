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
 * @file puzzle_generation_benchmark.cpp
 * @brief Configurable puzzle generation benchmark
 *
 * Replaces benchmark_phase2.cpp and benchmark_phase3_quick.cpp with a unified,
 * YAML-configurable benchmark runner.
 *
 * Usage:
 *   ./puzzle_generation_benchmark [options]
 *
 * Options:
 *   --config <file>     YAML config file (optional if --difficulty and --iterations provided)
 *   --difficulty <name> Override: Easy|Medium|Hard|Expert
 *   --iterations <n>    Override: number of puzzles to generate
 *   --verbose           Enable per-iteration output
 *   --help              Show this help message
 *
 * Examples:
 *   # Run with config file
 *   ./puzzle_generation_benchmark --config configs/standard.yaml
 *
 *   # Run without config file (CLI arguments only)
 *   ./puzzle_generation_benchmark --difficulty Hard --iterations 10 --verbose
 *
 *   # Override config file settings
 *   ./puzzle_generation_benchmark --config configs/quick.yaml --difficulty Expert
 */

#include "core/game_validator.h"
#include "core/puzzle_generator.h"
#include "helpers/benchmark_utils.h"

#include <cstring>
#include <iostream>

using namespace sudoku;

/// Command-line argument structure
struct CommandLineArgs {
    std::optional<std::string> config_file;
    std::optional<core::Difficulty> difficulty_override;
    std::optional<int> iterations_override;
    bool verbose = false;
    bool show_help = false;
};

/// Parse difficulty string to enum
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

/// Parse command-line arguments
static CommandLineArgs parseArgs(int argc, char* argv[]) {
    CommandLineArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            args.show_help = true;
        } else if (arg == "--config" && i + 1 < argc) {
            args.config_file = argv[++i];
        } else if (arg == "--difficulty" && i + 1 < argc) {
            std::string diff_str = argv[++i];
            auto diff = parseDifficulty(diff_str);
            if (!diff.has_value()) {
                std::cerr << "Error: Invalid difficulty '" << diff_str << "'. Expected: Easy, Medium, Hard, Expert\n";
                std::exit(1);
            }
            args.difficulty_override = diff;
        } else if (arg == "--iterations" && i + 1 < argc) {
            try {
                int iters = std::stoi(argv[++i]);
                if (iters <= 0) {
                    std::cerr << "Error: Iterations must be positive\n";
                    std::exit(1);
                }
                args.iterations_override = iters;
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid iterations value\n";
                std::exit(1);
            }
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        } else {
            std::cerr << "Error: Unknown argument '" << arg << "'\n";
            std::cerr << "Use --help for usage information\n";
            std::exit(1);
        }
    }

    return args;
}

/// Print help message
static void printHelp(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --config <file>     YAML config file (optional if --difficulty and --iterations provided)\n";
    std::cout << "  --difficulty <name> Difficulty level (Easy|Medium|Hard|Expert)\n";
    std::cout << "  --iterations <n>    Number of puzzles to generate\n";
    std::cout << "  --verbose, -v       Enable per-iteration output\n";
    std::cout << "  --help, -h          Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  # Run with config file\n";
    std::cout << "  " << program_name << " --config configs/standard.yaml\n\n";
    std::cout << "  # Run without config file (CLI arguments only)\n";
    std::cout << "  " << program_name << " --difficulty Hard --iterations 10 --verbose\n\n";
    std::cout << "  # Override config file settings\n";
    std::cout << "  " << program_name << " --config configs/quick.yaml --difficulty Expert\n";
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    auto args = parseArgs(argc, argv);

    if (args.show_help) {
        printHelp(argv[0]);
        return 0;
    }

    testing::BenchmarkConfig config;
    bool config_loaded = false;

    // Determine if we can skip config file loading
    bool can_skip_config = args.difficulty_override.has_value() && args.iterations_override.has_value();

    if (args.config_file.has_value()) {
        // Config file provided - load it
        auto config_result = testing::BenchmarkConfig::fromYAML(*args.config_file);
        if (!config_result.has_value()) {
            std::cerr << "Error loading config: " << config_result.error() << "\n";
            std::cerr << "Config file: " << *args.config_file << "\n";
            return 1;
        }
        config = config_result.value();
        config_loaded = true;

        // Apply command-line overrides
        if (args.difficulty_override) {
            config.difficulty = *args.difficulty_override;
            switch (*args.difficulty_override) {
                case core::Difficulty::Easy:
                    config.difficulty_name = "Easy";
                    break;
                case core::Difficulty::Medium:
                    config.difficulty_name = "Medium";
                    break;
                case core::Difficulty::Hard:
                    config.difficulty_name = "Hard";
                    break;
                case core::Difficulty::Expert:
                    config.difficulty_name = "Expert";
                    break;
                case core::Difficulty::Master:
                    config.difficulty_name = "Master";
                    break;
            }
        }

        if (args.iterations_override) {
            config.iterations = *args.iterations_override;
        }

        if (args.verbose) {
            config.verbose = true;
        }
    } else if (can_skip_config) {
        // No config file, but both difficulty and iterations provided via CLI
        config.difficulty = *args.difficulty_override;
        config.iterations = *args.iterations_override;
        config.verbose = args.verbose;

        switch (*args.difficulty_override) {
            case core::Difficulty::Easy:
                config.difficulty_name = "Easy";
                break;
            case core::Difficulty::Medium:
                config.difficulty_name = "Medium";
                break;
            case core::Difficulty::Hard:
                config.difficulty_name = "Hard";
                break;
            case core::Difficulty::Expert:
                config.difficulty_name = "Expert";
                break;
            case core::Difficulty::Master:
                config.difficulty_name = "Master";
                break;
        }
    } else {
        // Neither config file nor sufficient CLI arguments provided
        std::cerr << "Error: Insufficient arguments.\n\n";
        std::cerr << "Either provide:\n";
        std::cerr << "  1. A config file: --config <file>\n";
        std::cerr << "  2. Both --difficulty and --iterations\n\n";
        std::cerr << "Use --help for more information.\n";
        return 1;
    }

    // Print header
    std::cout << "=================================================================\n";
    std::cout << "Puzzle Generation Benchmark\n";
    std::cout << "=================================================================\n";
    if (config_loaded) {
        std::cout << "Configuration: " << *args.config_file << "\n";
    } else {
        std::cout << "Configuration: CLI arguments (no config file)\n";
    }
    std::cout << "Difficulty:    " << config.difficulty_name << "\n";
    std::cout << "Iterations:    " << config.iterations << "\n";
    std::cout << "=================================================================\n";

    // Initialize generator and validator
    core::GameValidator validator;
    core::PuzzleGenerator generator;

    // Run benchmark
    auto result = testing::runBenchmark(generator, config);

    // Print results
    std::cout << "\n=================================================================\n";
    std::cout << "BENCHMARK RESULTS\n";
    std::cout << "=================================================================\n";
    result.printSummary(std::cout);
    std::cout << "=================================================================\n";

    // Exit with non-zero if success rate < 90%
    if (result.success_rate < 90.0) {
        std::cerr << "\nWARNING: Success rate below 90% threshold!\n";
        return 1;
    }

    return 0;
}
