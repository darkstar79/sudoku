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
 * @file solver_path_benchmark.cpp
 * @brief Head-to-head comparison of Scalar, AVX2, and AVX-512 solver paths.
 *
 * Generates puzzles using identical deterministic seeds across all available
 * SIMD paths to enable fair comparison. Reports timing statistics and
 * speedup relative to the scalar path.
 *
 * Usage:
 *   ./solver_path_benchmark [options]
 *
 * Options:
 *   --difficulty <name>  Easy|Medium|Hard|Expert (default: Hard)
 *   --iterations <n>     Number of puzzles per path (default: 500)
 *   --verbose            Print per-iteration timing
 *   --help               Show this help message
 *
 * Example:
 *   ./solver_path_benchmark --difficulty Hard --iterations 500
 */

#include "core/cpu_features.h"
#include "core/puzzle_generator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

using namespace sudoku::core;

struct PathResult {
    SolverPath path;
    bool available{false};
    int iterations{0};
    int successes{0};
    double avg_ms{0.0};
    double median_ms{0.0};
    double p25_ms{0.0};
    double p75_ms{0.0};
    double min_ms{0.0};
    double max_ms{0.0};
    double std_dev_ms{0.0};
    double total_s{0.0};
    std::vector<double> times;
};

struct CommandLineArgs {
    Difficulty difficulty{Difficulty::Hard};
    std::string difficulty_name{"Hard"};
    int iterations{1000};
    int warmup{50};
    int repeats{3};
    bool verbose{false};
    bool show_help{false};
};

static std::optional<Difficulty> parseDifficulty(const std::string& str) {
    if (str == "Easy") {
        return Difficulty::Easy;
    }
    if (str == "Medium") {
        return Difficulty::Medium;
    }
    if (str == "Hard") {
        return Difficulty::Hard;
    }
    if (str == "Expert") {
        return Difficulty::Expert;
    }
    if (str == "Master") {
        return Difficulty::Master;
    }
    return std::nullopt;
}

static CommandLineArgs parseArgs(int argc, char* argv[]) {
    CommandLineArgs args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            args.show_help = true;
        } else if (arg == "--difficulty" && i + 1 < argc) {
            std::string diff_str = argv[++i];
            auto diff = parseDifficulty(diff_str);
            if (!diff.has_value()) {
                std::cerr << "Error: Invalid difficulty '" << diff_str << "'\n";
                std::exit(1);
            }
            args.difficulty = *diff;
            args.difficulty_name = diff_str;
        } else if (arg == "--iterations" && i + 1 < argc) {
            args.iterations = std::stoi(argv[++i]);
            if (args.iterations <= 0) {
                std::cerr << "Error: Iterations must be positive\n";
                std::exit(1);
            }
        } else if (arg == "--warmup" && i + 1 < argc) {
            args.warmup = std::stoi(argv[++i]);
            if (args.warmup < 0) {
                std::cerr << "Error: Warmup must be non-negative\n";
                std::exit(1);
            }
        } else if (arg == "--repeats" && i + 1 < argc) {
            args.repeats = std::stoi(argv[++i]);
            if (args.repeats <= 0) {
                std::cerr << "Error: Repeats must be positive\n";
                std::exit(1);
            }
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        } else {
            std::cerr << "Error: Unknown argument '" << arg << "'\n";
            std::exit(1);
        }
    }

    return args;
}

static PathResult runPath(PuzzleGenerator& generator, SolverPath path, Difficulty difficulty, int iterations,
                          int warmup, bool verbose) {
    PathResult result;
    result.path = path;
    result.available = isSolverPathAvailable(path);
    result.iterations = iterations;

    if (!result.available) {
        return result;
    }

    generator.setSolverPath(path);

    // Warmup: generate puzzles without timing to fill caches
    for (int i = 0; i < warmup; ++i) {
        GenerationSettings settings;
        settings.difficulty = difficulty;
        settings.seed = static_cast<uint32_t>(i + 1);
        settings.solver_path = path;
        auto puzzle = generator.generatePuzzle(settings);
        (void)puzzle;
    }

    result.times.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        // Use deterministic seed: iteration index + 1 (same across all paths)
        GenerationSettings settings;
        settings.difficulty = difficulty;
        settings.seed = static_cast<uint32_t>(i + 1);
        settings.solver_path = path;

        auto start = std::chrono::high_resolution_clock::now();
        auto puzzle = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.times.push_back(ms);

        if (puzzle.has_value()) {
            result.successes++;
        }

        if (verbose) {
            std::cout << "  [" << solverPathName(path) << "] " << std::setw(3) << (i + 1) << "/" << iterations << "  "
                      << std::fixed << std::setprecision(1) << ms << "ms" << (puzzle.has_value() ? "" : " FAILED")
                      << "\n";
        }
    }

    // Compute statistics
    if (!result.times.empty()) {
        double sum = std::accumulate(result.times.begin(), result.times.end(), 0.0);
        result.avg_ms = sum / result.times.size();
        result.total_s = sum / 1000.0;

        std::vector<double> sorted = result.times;
        std::sort(sorted.begin(), sorted.end());
        result.median_ms = sorted[sorted.size() / 2];
        result.p25_ms = sorted[sorted.size() / 4];
        result.p75_ms = sorted[(sorted.size() * 3) / 4];
        result.min_ms = sorted.front();
        result.max_ms = sorted.back();

        // Standard deviation
        double sq_sum = 0.0;
        for (double t : result.times) {
            double diff = t - result.avg_ms;
            sq_sum += diff * diff;
        }
        result.std_dev_ms = std::sqrt(sq_sum / result.times.size());
    }

    return result;
}

int main(int argc, char* argv[]) {
    auto args = parseArgs(argc, argv);

    if (args.show_help) {
        std::cout << "Usage: solver_path_benchmark [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --difficulty <name>  Easy|Medium|Hard|Expert (default: Hard)\n";
        std::cout << "  --iterations <n>     Puzzles per path (default: 1000)\n";
        std::cout << "  --warmup <n>         Warmup puzzles before timing (default: 50)\n";
        std::cout << "  --repeats <n>        Repeat full run N times, keep best (default: 3)\n";
        std::cout << "  --verbose, -v        Per-iteration output\n";
        std::cout << "  --help, -h           Show this help\n";
        return 0;
    }

    // Print CPU capabilities
    const auto& cpu = getCpuFeatures();
    std::cout << "CPU Features: AVX2=" << (cpu.has_avx2 ? "yes" : "no")
              << "  AVX-512F=" << (cpu.has_avx512f ? "yes" : "no")
              << "  AVX-512BW=" << (cpu.has_avx512bw ? "yes" : "no") << "  POPCNT=" << (cpu.has_popcnt ? "yes" : "no")
              << "\n\n";

    std::cout << "Solver Path Comparison (" << args.difficulty_name << ", " << args.iterations << " iterations x "
              << args.repeats << " repeats, " << args.warmup << " warmup)\n";
    std::cout << "================================================================\n";

    PuzzleGenerator generator;

    // Run each path with repeats (keep best run to reduce OS scheduler noise)
    constexpr std::array<SolverPath, 3> paths = {SolverPath::Scalar, SolverPath::AVX2, SolverPath::AVX512};
    std::vector<PathResult> results;

    for (SolverPath path : paths) {
        std::cout << "\nRunning " << solverPathName(path) << "..." << std::flush;
        if (!isSolverPathAvailable(path)) {
            std::cout << " SKIPPED (not available on this CPU)\n";
            PathResult skipped;
            skipped.path = path;
            skipped.available = false;
            results.push_back(skipped);
            continue;
        }
        std::cout << "\n";

        PathResult best;
        best.avg_ms = 1e9;

        for (int rep = 0; rep < args.repeats; ++rep) {
            auto result = runPath(generator, path, args.difficulty, args.iterations, args.warmup, args.verbose);
            std::cout << "  Rep " << (rep + 1) << "/" << args.repeats << ": " << std::fixed << std::setprecision(2)
                      << result.avg_ms << "ms avg, " << result.median_ms << "ms median\n";
            if (result.avg_ms < best.avg_ms) {
                best = result;
            }
        }

        results.push_back(best);
        std::cout << "  Best: " << std::fixed << std::setprecision(2) << best.total_s << "s total, " << best.avg_ms
                  << "ms avg\n";
    }

    // Find scalar baseline for speedup calculation
    double scalar_avg = 0.0;
    for (const auto& r : results) {
        if (r.path == SolverPath::Scalar && r.available) {
            scalar_avg = r.avg_ms;
            break;
        }
    }

    // Print summary table
    std::cout << "\n================================================================\n";
    std::cout << "RESULTS (best of " << args.repeats << " repeats)\n";
    std::cout << "================================================================\n";
    std::cout << std::left << std::setw(10) << "Path" << std::right << std::setw(9) << "Avg(ms)" << std::setw(9)
              << "Median" << std::setw(9) << "P25" << std::setw(9) << "P75" << std::setw(9) << "StdDev" << std::setw(9)
              << "Total(s)" << std::setw(9) << "Speedup"
              << "\n";
    std::cout << "---------- -------- -------- -------- -------- -------- -------- --------\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(10) << solverPathName(r.path);

        if (!r.available) {
            std::cout << "  SKIPPED (not available)\n";
            continue;
        }

        std::cout << std::right << std::fixed << std::setprecision(2) << std::setw(9) << r.avg_ms << std::setw(9)
                  << r.median_ms << std::setw(9) << r.p25_ms << std::setw(9) << r.p75_ms << std::setw(9) << r.std_dev_ms
                  << std::setw(9) << std::setprecision(1) << r.total_s;

        if (scalar_avg > 0.0 && r.avg_ms > 0.0) {
            std::cout << std::setw(7) << std::setprecision(2) << (scalar_avg / r.avg_ms) << "x";
        }

        std::cout << "\n";
    }

    std::cout << "================================================================\n";

    return 0;
}
