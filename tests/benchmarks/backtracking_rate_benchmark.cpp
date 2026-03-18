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
 * @file backtracking_rate_benchmark.cpp
 * @brief Measures backtracking rate and technique coverage across difficulty levels.
 *
 * Generates puzzles at each difficulty, rates them using the logical solver,
 * and reports what percentage require backtracking. Also tracks which techniques
 * appear in solve paths, reconstructs the board state at the stall point, and
 * analyzes candidate density to help diagnose which technique families are missing.
 *
 * Usage:
 *   ./backtracking_rate_benchmark [options]
 *
 * Options:
 *   --difficulty <name>  Easy|Medium|Hard|Expert|All (default: All)
 *   --iterations <n>     Puzzles per difficulty (default: 200)
 *   --samples <n>        Stall board samples to print per difficulty (default: 3)
 *   --verbose            Print per-puzzle details
 *   --help               Show this help message
 *
 * Example:
 *   ./backtracking_rate_benchmark --difficulty Hard --iterations 200
 */

#include "core/candidate_grid.h"
#include "core/game_validator.h"
#include "core/puzzle_generator.h"
#include "core/puzzle_rater.h"
#include "core/solving_technique.h"
#include "core/sudoku_solver.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include <spdlog/spdlog.h>

using namespace sudoku::core;

/// Snapshot of the board and candidate state at the stall point
struct StallSnapshot {
    BoardData board;             ///< Board state when logic stalled
    int empty_cells{0};          ///< Cells still empty at stall
    int filled_by_logic{0};      ///< Cells filled by logical techniques
    double avg_candidates{0.0};  ///< Average candidates per empty cell at stall
    int seed{0};                 ///< Puzzle seed for reproducibility
};

struct PuzzleAnalysis {
    int rating{0};
    int clue_count{0};
    bool requires_backtracking{false};
    std::set<SolvingTechnique> techniques_used;
    SolvingTechnique last_technique_before_bt{SolvingTechnique::Backtracking};
    std::optional<StallSnapshot> stall;  ///< Present only if backtracking was needed
};

struct DifficultyStats {
    Difficulty difficulty;
    std::string name;
    int total{0};
    int successes{0};
    int backtracking_count{0};
    double total_rating{0.0};
    double total_clues{0.0};
    double total_time_ms{0.0};
    std::map<SolvingTechnique, int> technique_frequency;
    std::map<SolvingTechnique, int> last_before_bt;
    // Stall depth tracking
    double total_stall_empty{0.0};
    double total_stall_candidates{0.0};
    std::vector<StallSnapshot> stall_samples;  ///< First N samples for display
};

struct CommandLineArgs {
    std::vector<Difficulty> difficulties;
    std::vector<std::string> difficulty_names;
    int iterations{200};
    int max_samples{3};
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
    return std::nullopt;
}

static std::string difficultyName(Difficulty d) {
    switch (d) {
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

static CommandLineArgs parseArgs(int argc, char* argv[]) {
    CommandLineArgs args;

    bool difficulty_set = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            args.show_help = true;
        } else if (arg == "--difficulty" && i + 1 < argc) {
            std::string diff_str = argv[++i];
            if (diff_str == "All") {
                difficulty_set = true;
                args.difficulties = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert,
                                     Difficulty::Master};
                args.difficulty_names = {"Easy", "Medium", "Hard", "Expert", "Master"};
            } else {
                auto diff = parseDifficulty(diff_str);
                if (!diff.has_value()) {
                    std::cerr << "Error: Invalid difficulty '" << diff_str << "'\n";
                    std::exit(1);
                }
                difficulty_set = true;
                args.difficulties = {*diff};
                args.difficulty_names = {diff_str};
            }
        } else if (arg == "--iterations" && i + 1 < argc) {
            args.iterations = std::stoi(argv[++i]);
            if (args.iterations <= 0) {
                std::cerr << "Error: Iterations must be positive\n";
                std::exit(1);
            }
        } else if (arg == "--samples" && i + 1 < argc) {
            args.max_samples = std::stoi(argv[++i]);
            if (args.max_samples < 0) {
                std::cerr << "Error: Samples must be non-negative\n";
                std::exit(1);
            }
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        } else {
            std::cerr << "Error: Unknown argument '" << arg << "'\n";
            std::exit(1);
        }
    }

    if (!difficulty_set) {
        args.difficulties = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert};
        args.difficulty_names = {"Easy", "Medium", "Hard", "Expert"};
    }

    return args;
}

/// Reconstruct the board state at the stall point by replaying logical steps
static StallSnapshot buildStallSnapshot(const BoardData& original_board, const std::vector<SolveStep>& solve_path,
                                        int seed) {
    StallSnapshot snap;
    snap.seed = seed;

    // Copy original board and replay placements + eliminations
    auto board = original_board;
    CandidateGrid candidates(board);

    for (const auto& step : solve_path) {
        if (step.technique == SolvingTechnique::Backtracking) {
            break;  // Stop at backtracking
        }
        if (step.type == SolveStepType::Placement) {
            board[step.position.row][step.position.col] = step.value;
            candidates.placeValue(step.position.row, step.position.col, step.value);
        } else {
            for (const auto& elim : step.eliminations) {
                candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
            }
        }
    }

    snap.board = board;

    // Count clues in original
    int original_clues = 0;
    for (const auto& row : original_board) {
        for (int cell : row) {
            if (cell != 0) {
                original_clues++;
            }
        }
    }

    // Count empty cells at stall and compute candidate stats
    int total_candidates = 0;
    int filled_now = 0;
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] != 0) {
                filled_now++;
            } else {
                snap.empty_cells++;
                total_candidates += candidates.countPossibleValues(row, col);
            }
        }
    }

    snap.filled_by_logic = filled_now - original_clues;
    snap.avg_candidates = snap.empty_cells > 0 ? static_cast<double>(total_candidates) / snap.empty_cells : 0.0;

    return snap;
}

static PuzzleAnalysis analyzePuzzle(PuzzleRater& rater, const BoardData& board, int clue_count, int seed) {
    PuzzleAnalysis analysis;
    analysis.clue_count = clue_count;

    auto rating_result = rater.ratePuzzle(board);
    if (!rating_result.has_value()) {
        analysis.requires_backtracking = true;
        return analysis;
    }

    analysis.rating = rating_result->total_score;
    analysis.requires_backtracking = rating_result->requires_backtracking;

    for (const auto& step : rating_result->solve_path) {
        analysis.techniques_used.insert(step.technique);
    }

    // Find last logical technique before backtracking
    if (analysis.requires_backtracking) {
        const auto& path = rating_result->solve_path;
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            if (it->technique != SolvingTechnique::Backtracking) {
                analysis.last_technique_before_bt = it->technique;
                break;
            }
        }

        // Build stall snapshot
        analysis.stall = buildStallSnapshot(board, rating_result->solve_path, seed);
    }

    return analysis;
}

static DifficultyStats runDifficulty(PuzzleGenerator& generator, PuzzleRater& rater, Difficulty difficulty,
                                     int iterations, int max_samples, bool verbose) {
    DifficultyStats stats;
    stats.difficulty = difficulty;
    stats.name = difficultyName(difficulty);

    std::cout << "\nGenerating " << iterations << " " << stats.name << " puzzles..." << std::flush;
    if (verbose) {
        std::cout << "\n";
    }

    for (int i = 0; i < iterations; ++i) {
        stats.total++;

        GenerationSettings settings;
        settings.difficulty = difficulty;
        settings.seed = static_cast<uint32_t>(i + 1);

        auto start = std::chrono::high_resolution_clock::now();
        auto puzzle = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        stats.total_time_ms += ms;

        if (!puzzle.has_value()) {
            if (verbose) {
                std::cout << "  [" << std::setw(3) << (i + 1) << "/" << iterations << "] FAILED to generate\n";
            }
            continue;
        }

        stats.successes++;
        auto analysis = analyzePuzzle(rater, puzzle->board, puzzle->clue_count, i + 1);
        stats.total_rating += analysis.rating;
        stats.total_clues += analysis.clue_count;

        if (analysis.requires_backtracking) {
            stats.backtracking_count++;
            stats.last_before_bt[analysis.last_technique_before_bt]++;

            if (analysis.stall.has_value()) {
                stats.total_stall_empty += analysis.stall->empty_cells;
                stats.total_stall_candidates += analysis.stall->avg_candidates;

                if (static_cast<int>(stats.stall_samples.size()) < max_samples) {
                    stats.stall_samples.push_back(*analysis.stall);
                }
            }
        }

        for (const auto& tech : analysis.techniques_used) {
            stats.technique_frequency[tech]++;
        }

        if (verbose) {
            std::cout << "  [" << std::setw(3) << (i + 1) << "/" << iterations << "] " << std::setw(5)
                      << analysis.rating << " pts  " << analysis.clue_count << " clues";
            if (analysis.requires_backtracking && analysis.stall.has_value()) {
                std::cout << "  ** BT ** " << analysis.stall->empty_cells << " empty, " << std::fixed
                          << std::setprecision(1) << analysis.stall->avg_candidates << " avg cands";
            }
            std::cout << "\n";
        }
    }

    if (!verbose) {
        double bt_rate = stats.successes > 0 ? 100.0 * stats.backtracking_count / stats.successes : 0.0;
        std::cout << " done (" << std::fixed << std::setprecision(1) << bt_rate << "% backtracking)\n";
    }

    return stats;
}

static void printSummaryTable(const std::vector<DifficultyStats>& all_stats) {
    std::cout << "\n=== Backtracking Rate Analysis ===\n";
    std::cout << std::left << std::setw(10) << "Difficulty" << std::right << std::setw(9) << "Puzzles" << std::setw(10)
              << "BT Rate" << std::setw(12) << "Avg Rating" << std::setw(11) << "Avg Clues" << std::setw(12)
              << "Avg GenTime" << std::setw(12) << "Avg Empty" << std::setw(11) << "Avg Cands"
              << "\n";
    std::cout << "---------- -------- --------- ----------- ---------- -----------"
                 " ----------- ----------\n";

    for (const auto& stats : all_stats) {
        double bt_rate = stats.successes > 0 ? 100.0 * stats.backtracking_count / stats.successes : 0.0;
        double avg_rating = stats.successes > 0 ? stats.total_rating / stats.successes : 0.0;
        double avg_clues = stats.successes > 0 ? stats.total_clues / stats.successes : 0.0;
        double avg_time = stats.successes > 0 ? stats.total_time_ms / stats.successes : 0.0;
        double avg_empty = stats.backtracking_count > 0 ? stats.total_stall_empty / stats.backtracking_count : 0.0;
        double avg_cands = stats.backtracking_count > 0 ? stats.total_stall_candidates / stats.backtracking_count : 0.0;

        std::cout << std::left << std::setw(10) << stats.name << std::right << std::setw(9) << stats.successes
                  << std::fixed << std::setprecision(1) << std::setw(9) << bt_rate << "%" << std::setw(12)
                  << static_cast<int>(avg_rating) << std::setw(11) << std::setprecision(1) << avg_clues << std::setw(9)
                  << std::setprecision(0) << avg_time << "ms";

        if (stats.backtracking_count > 0) {
            std::cout << std::setw(10) << std::setprecision(1) << avg_empty << std::setw(11) << std::setprecision(1)
                      << avg_cands;
        } else {
            std::cout << std::setw(10) << "-" << std::setw(11) << "-";
        }
        std::cout << "\n";
    }
}

static void printTechniqueFrequency(const DifficultyStats& stats) {
    if (stats.technique_frequency.empty()) {
        return;
    }

    std::cout << "\n=== Technique Frequency (" << stats.name << ", " << stats.successes << " puzzles) ===\n";

    // Sort by frequency (descending)
    std::vector<std::pair<SolvingTechnique, int>> sorted_techs(stats.technique_frequency.begin(),
                                                               stats.technique_frequency.end());
    std::sort(sorted_techs.begin(), sorted_techs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [tech, count] : sorted_techs) {
        double pct = stats.successes > 0 ? 100.0 * count / stats.successes : 0.0;
        std::cout << "  " << std::left << std::setw(22) << getTechniqueName(tech) << std::right << ": " << std::setw(5)
                  << count << " (" << std::fixed << std::setprecision(1) << std::setw(5) << pct << "%)\n";
    }
}

static void printStallAnalysis(const DifficultyStats& stats) {
    if (stats.last_before_bt.empty()) {
        return;
    }

    std::cout << "\n=== Backtracking Stall Analysis (" << stats.name << ", " << stats.backtracking_count
              << " BT puzzles) ===\n";
    std::cout << "Last technique before backtracking:\n";

    // Sort by frequency (descending)
    std::vector<std::pair<SolvingTechnique, int>> sorted(stats.last_before_bt.begin(), stats.last_before_bt.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [tech, count] : sorted) {
        double pct = stats.backtracking_count > 0 ? 100.0 * count / stats.backtracking_count : 0.0;
        std::cout << "  " << std::left << std::setw(22) << getTechniqueName(tech) << std::right << ": " << std::setw(5)
                  << count << " (" << std::fixed << std::setprecision(1) << std::setw(5) << pct << "%)\n";
    }
}

/// Print a compact 9x9 board (. for empty, digit for filled)
static void printCompactBoard(const BoardData& board) {
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        if (row == 3 || row == 6) {
            std::cout << "  ------+-------+------\n";
        }
        std::cout << "  ";
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (col == 3 || col == 6) {
                std::cout << "| ";
            }
            int val = board[row][col];
            if (val == 0) {
                std::cout << ". ";
            } else {
                std::cout << val << " ";
            }
        }
        std::cout << "\n";
    }
}

/// Print candidate lists for empty cells (up to max_shown cells)
static void printCandidateLists(const BoardData& board, int empty_cells, int max_shown = 20) {
    std::cout << "  Candidates at stall:\n";
    CandidateGrid cands(board);
    int shown = 0;
    for (size_t row = 0; row < BOARD_SIZE && shown < max_shown; ++row) {
        for (size_t col = 0; col < BOARD_SIZE && shown < max_shown; ++col) {
            if (board[row][col] == 0) {
                uint16_t mask = cands.getPossibleValuesMask(row, col);
                std::cout << "    r" << row << "c" << col << ": {";
                bool first = true;
                for (int val = 1; val <= 9; ++val) {
                    if ((mask & static_cast<uint16_t>(1 << val)) != 0) {
                        if (!first) {
                            std::cout << ",";
                        }
                        std::cout << val;
                        first = false;
                    }
                }
                std::cout << "}\n";
                shown++;
            }
        }
    }
    if (empty_cells > max_shown) {
        std::cout << "    ... (" << (empty_cells - max_shown) << " more empty cells)\n";
    }
}

/// Print compact stall board samples showing the board state where logic gave up
static void printStallBoardSamples(const DifficultyStats& stats) {
    if (stats.stall_samples.empty()) {
        return;
    }

    std::cout << "\n=== Stall Board Samples (" << stats.name << ", showing " << stats.stall_samples.size() << " of "
              << stats.backtracking_count << " BT puzzles) ===\n";

    for (size_t si = 0; si < stats.stall_samples.size(); ++si) {
        const auto& snap = stats.stall_samples[si];
        std::cout << "\n--- Sample " << (si + 1) << " (seed=" << snap.seed << ", empty=" << snap.empty_cells
                  << ", filled_by_logic=" << snap.filled_by_logic << ", avg_candidates=" << std::fixed
                  << std::setprecision(1) << snap.avg_candidates << ") ---\n";

        printCompactBoard(snap.board);
        printCandidateLists(snap.board, snap.empty_cells);
    }
}

int main(int argc, char* argv[]) {
    auto args = parseArgs(argc, argv);

    if (args.show_help) {
        std::cout << "Usage: backtracking_rate_benchmark [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --difficulty <name>  Easy|Medium|Hard|Expert|All (default: All)\n";
        std::cout << "  --iterations <n>     Puzzles per difficulty (default: 200)\n";
        std::cout << "  --samples <n>        Stall board samples per difficulty (default: 3)\n";
        std::cout << "  --verbose, -v        Per-puzzle details\n";
        std::cout << "  --help, -h           Show this help\n";
        return 0;
    }

    // Suppress spdlog noise during benchmark
    spdlog::set_level(spdlog::level::err);

    // Compose solver pipeline
    auto validator = std::make_shared<GameValidator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto rater = std::make_shared<PuzzleRater>(solver);
    PuzzleGenerator generator(rater);

    std::cout << "Backtracking Rate Benchmark (" << args.iterations << " puzzles per difficulty)\n";
    std::cout << "================================================================\n";
    std::cout << "Strategies: 31 logical + backtracking fallback\n";

    std::vector<DifficultyStats> all_stats;
    for (size_t d = 0; d < args.difficulties.size(); ++d) {
        auto stats =
            runDifficulty(generator, *rater, args.difficulties[d], args.iterations, args.max_samples, args.verbose);
        all_stats.push_back(std::move(stats));
    }

    // Summary table
    printSummaryTable(all_stats);

    // Per-difficulty detail for requested difficulties
    for (const auto& stats : all_stats) {
        printTechniqueFrequency(stats);
        printStallAnalysis(stats);
        printStallBoardSamples(stats);
    }

    std::cout << "\n================================================================\n";

    return 0;
}
