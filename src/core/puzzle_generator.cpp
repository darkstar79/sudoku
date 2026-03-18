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

#include "puzzle_generator.h"

#include "backtracking_solver.h"
#include "core/board.h"
#include "core/board_utils.h"
#include "core/constants.h"
#include "core/game_validator.h"
#include "core/i_puzzle_generator.h"
#include "core/i_puzzle_rater.h"
#include "core/solve_step.h"
#include "core/solving_technique.h"
#include "puzzle_rating.h"
#include "solution_counter.h"

#include <algorithm>
#include <array>
#include <random>
#include <set>
#include <utility>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::core {

// Clue count ranges for each difficulty level
namespace {
constexpr int EASY_MIN_CLUES = 36;
constexpr int EASY_MAX_CLUES = 40;
constexpr int MEDIUM_MIN_CLUES = 28;
constexpr int MEDIUM_MAX_CLUES = 35;
constexpr int HARD_MIN_CLUES = 22;
constexpr int HARD_MAX_CLUES = 27;
constexpr int EXPERT_MIN_CLUES = 17;
constexpr int EXPERT_MAX_CLUES = 27;  // Clue count doesn't determine difficulty — rating does
constexpr int MASTER_MIN_CLUES = 17;
constexpr int MASTER_MAX_CLUES = 27;  // Same as Expert — rating determines difficulty

}  // namespace

PuzzleGenerator::PuzzleGenerator() = default;

PuzzleGenerator::PuzzleGenerator(std::shared_ptr<IPuzzleRater> rater) : rater_(std::move(rater)) {
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — top-level orchestration over difficulty-specific strategies; branching is inherent to the generation flow
std::expected<Puzzle, GenerationError> PuzzleGenerator::generatePuzzle(const GenerationSettings& settings) const {
    // Validate input
    if (settings.difficulty < Difficulty::Easy || settings.difficulty > Difficulty::Master) {
        spdlog::error("Invalid difficulty: {}", static_cast<int>(settings.difficulty));
        return std::unexpected(GenerationError::InvalidDifficulty);
    }

    // Apply solver path from settings (if specified)
    solution_counter_.setSolverPath(settings.solver_path.value_or(SolverPath::Auto));

    spdlog::debug("Generating puzzle with difficulty: {}, ensure_unique: {}, max_attempts: {}, solver_path: {}",
                  static_cast<int>(settings.difficulty), settings.ensure_unique, settings.max_attempts,
                  solverPathName(solution_counter_.solverPath()));

    // Set up random number generator
    std::mt19937 rng(settings.seed != 0 ? settings.seed : std::random_device{}());

    // Retry loop for puzzle generation
    for (int attempt = 0; attempt < settings.max_attempts; ++attempt) {
        // Clear memoization cache once per attempt (not per countSolutions call)
        // This allows cache to accumulate during clue removal within a single attempt
        solution_counter_.clearCache();

        // Generate complete solution
        spdlog::debug("Attempt {}/{}: Generating complete solution...", attempt + 1, settings.max_attempts);
        auto solution = generateCompleteSolution(rng);
        if (!solution.has_value()) {
            spdlog::error("Failed to generate complete solution on attempt {}", attempt + 1);
            continue;  // Try again
        }
        spdlog::debug("Complete solution generated successfully");

        // Remove clues to create puzzle
        // Use iterative deepening for Expert, standard greedy for others
        spdlog::debug("Removing clues to create puzzle...");
        auto puzzle_board = removeCluesToCreatePuzzleIterative(*solution, settings, rng);
        if (!puzzle_board.has_value()) {
            spdlog::error("Failed to remove clues to create puzzle on attempt {}", attempt + 1);
            continue;  // Try again
        }
        int final_clue_count = countClues(*puzzle_board);
        spdlog::debug("Clues removed successfully, final clue count: {}, checking unique solution...",
                      final_clue_count);

        // Verify unique solution if required
        if (settings.ensure_unique && !hasUniqueSolution(*puzzle_board)) {
            spdlog::debug("Generated puzzle does not have unique solution (final clue count: {}), retrying...",
                          final_clue_count);
            continue;  // Try again
        }

        // Success! Create puzzle result
        spdlog::info("Successfully generated puzzle on attempt {}/{}", attempt + 1, settings.max_attempts);
        Puzzle result;
        result.board = *puzzle_board;
        result.solution = *solution;
        result.difficulty = settings.difficulty;
        result.seed = settings.seed;
        result.clue_count = countClues(result.board);

        // Rate puzzle if rater is available
        if (rater_) {
            auto rating_result = rater_->ratePuzzle(result.board);
            if (rating_result.has_value()) {
                result.rating = rating_result->total_score;
                result.requires_backtracking = rating_result->requires_backtracking;
                for (const auto& step : rating_result->solve_path) {
                    if (step.technique != SolvingTechnique::Backtracking) {
                        result.required_techniques.insert(step.technique);
                    }
                }
                spdlog::info("Puzzle rated: {} points (Sudoku Explainer scale)", result.rating);

                // Validate rating: reject puzzles outside the expected range for their difficulty
                {
                    auto [min_rating, max_rating] = getDifficultyRatingRange(settings.difficulty);
                    if (result.rating < min_rating || result.rating >= max_rating) {
                        spdlog::debug("Puzzle rating {} outside range [{}, {}), rejecting...", result.rating,
                                      min_rating, max_rating);
                        continue;  // Try again with different puzzle
                    }
                    spdlog::debug("Puzzle rating {} within range [{}, {})", result.rating, min_rating, max_rating);
                }
            } else {
                spdlog::warn("Failed to rate puzzle: {}", static_cast<int>(rating_result.error()));
                // Continue without rating if rating fails (don't reject puzzle)
            }
        }

        return result;
    }

    // All attempts failed
    spdlog::error("Failed to generate puzzle after {} attempts", settings.max_attempts);
    return std::unexpected(GenerationError::NoUniqueSolution);
}

std::expected<Puzzle, GenerationError> PuzzleGenerator::generatePuzzle(Difficulty difficulty) const {
    GenerationSettings settings;
    settings.difficulty = difficulty;
    settings.seed = 0;  // Random seed
    settings.ensure_unique = true;
    settings.max_attempts = 1000;

    return generatePuzzle(settings);
}

std::expected<BoardData, GenerationError> PuzzleGenerator::solvePuzzle(const BoardData& board) const {
    if (!validatePuzzle(board)) {
        return std::unexpected(GenerationError::GenerationFailed);
    }

    auto solution = board;  // Copy the input board
    if (solvePuzzleBacktrack(solution)) {
        return solution;
    }

    return std::unexpected(GenerationError::GenerationFailed);
}

bool PuzzleGenerator::hasUniqueSolution(const BoardData& board) const {
    // Phase 3: Propagation integration removed - creates too much overhead
    // The existing solver already applies hidden singles and naked singles efficiently
    return solution_counter_.hasUniqueSolution(board);
}

int PuzzleGenerator::countClues(const BoardData& board) const {
    int count = 0;
    for (const auto& row : board) {
        for (int cell : row) {
            if (cell != 0) {
                ++count;
            }
        }
    }
    return count;
}

bool PuzzleGenerator::validatePuzzle(const BoardData& board) const {
    // Check that all values are 0-9
    for (const auto& row : board) {
        for (int cell : row) {
            if (cell < 0 || cell > 9) {
                return false;
            }
        }
    }

    // Check that current state has no conflicts
    return validator_.validateBoard(board);
}

std::optional<BoardData> PuzzleGenerator::generateCompleteSolution(std::mt19937& rng) const {
    auto board = createEmptyBoard();

    if (fillBoardRecursively(board, rng)) {
        return board;
    }

    return {};  // Return empty board if generation failed
}

int PuzzleGenerator::runRemovalOrdering(BoardData& puzzle, const std::vector<std::pair<size_t, size_t>>& positions,
                                        bool ensure_unique, int min_clues, int /*max_clues*/) const {
    for (const auto& [row, col] : positions) {
        if (puzzle[row][col] == 0) {
            continue;  // Already empty
        }

        int original_value = puzzle[row][col];
        puzzle[row][col] = 0;  // Remove clue

        // Check uniqueness on every removal — not just near target.
        // Skipping checks above max_clues caused non-unique puzzles to slip through.
        if (ensure_unique && !hasUniqueSolution(puzzle)) {
            puzzle[row][col] = original_value;  // Restore — non-unique
            continue;
        }

        if (countClues(puzzle) <= min_clues) {
            break;  // Reached minimum, stop removing
        }
    }
    return countClues(puzzle);
}

void PuzzleGenerator::runRecompletionPhase(BoardData& best_puzzle, int& best_clue_count,
                                           std::vector<std::pair<size_t, size_t>>& positions, bool ensure_unique,
                                           int min_clues, int max_clues, int num_orderings, std::mt19937& rng) const {
    constexpr int MAX_RECOMPLETIONS = 5;

    auto tryOrdering = [&](const BoardData& start) {
        auto puzzle = start;
        std::shuffle(positions.begin(), positions.end(), rng);
        int final_clues = runRemovalOrdering(puzzle, positions, ensure_unique, min_clues, max_clues);
        spdlog::debug("Re-completion ordering: {} clues remaining (target: {}-{})", final_clues, min_clues, max_clues);
        if (final_clues < best_clue_count) {
            best_clue_count = final_clues;
            best_puzzle = puzzle;
        }
    };

    for (int recomp = 0; recomp < MAX_RECOMPLETIONS; ++recomp) {
        auto recomp_puzzle = best_puzzle;
        int clues_to_drop = 2 + static_cast<int>(rng() % 2);
        for (const auto& pos : selectCluesForDropping(recomp_puzzle, clues_to_drop, rng)) {
            recomp_puzzle[pos.row][pos.col] = 0;
        }
        spdlog::debug("Re-completion {}/{}: Dropped {} clues", recomp + 1, MAX_RECOMPLETIONS, clues_to_drop);

        auto new_solution_result = solvePuzzle(recomp_puzzle);
        if (!new_solution_result.has_value()) {
            spdlog::debug("Re-completion {}/{}: Failed to find new solution", recomp + 1, MAX_RECOMPLETIONS);
            continue;
        }

        const auto& new_solution = new_solution_result.value();
        spdlog::debug("Re-completion {}/{}: Found new solution, retrying removal", recomp + 1, MAX_RECOMPLETIONS);

        for (int ordering = 0; ordering < num_orderings; ++ordering) {
            tryOrdering(new_solution);
            if (best_clue_count >= min_clues && best_clue_count <= max_clues) {
                spdlog::debug("Re-completion: Target range achieved ({} clues)", best_clue_count);
                return;
            }
        }
    }
}

std::optional<BoardData> PuzzleGenerator::removeCluesToCreatePuzzle(const BoardData& solution,
                                                                    const GenerationSettings& settings,
                                                                    std::mt19937& rng) const {
    auto [min_clues, max_clues] = getClueRange(settings.difficulty);

    // Determine number of removal orderings based on difficulty
    // Hard/Expert: try multiple orderings to escape local maxima in greedy clue removal
    // Easy/Medium: single ordering (success rate already near 100%)
    int num_orderings = 1;
    if (settings.difficulty == Difficulty::Expert || settings.difficulty == Difficulty::Master) {
        num_orderings = 10;
    } else if (settings.difficulty == Difficulty::Hard) {
        num_orderings = 3;
    }

    BoardData best_puzzle = solution;
    int best_clue_count = BOARD_SIZE * BOARD_SIZE;  // Start at 81

    // Build position list once; re-shuffle each ordering iteration
    std::vector<std::pair<size_t, size_t>> positions;
    positions.reserve(BOARD_SIZE * BOARD_SIZE);
    forEachCell([&](size_t row, size_t col) { positions.emplace_back(row, col); });

    // Runs one ordering from a given solution; updates best_puzzle/best_clue_count
    auto tryOrdering = [&](const BoardData& start) {
        auto puzzle = start;
        std::shuffle(positions.begin(), positions.end(), rng);
        int final_clues = runRemovalOrdering(puzzle, positions, settings.ensure_unique, min_clues, max_clues);
        spdlog::debug("Clue removal ordering: {} clues remaining (target: {}-{})", final_clues, min_clues, max_clues);
        if (final_clues < best_clue_count) {
            best_clue_count = final_clues;
            best_puzzle = puzzle;
        }
    };

    // Try multiple removal orderings
    for (int ordering = 0; ordering < num_orderings; ++ordering) {
        tryOrdering(solution);
        if (best_clue_count >= min_clues && best_clue_count <= max_clues) {
            spdlog::debug("Target clue range achieved ({} clues), stopping after {} orderings", best_clue_count,
                          ordering + 1);
            break;
        }
    }

    // Re-completion strategy: If stuck above max_clues, try finding new solution paths
    // Only enabled for Expert difficulty to avoid unnecessary work for easier puzzles
    if ((settings.difficulty == Difficulty::Expert || settings.difficulty == Difficulty::Master) &&
        best_clue_count > max_clues) {
        spdlog::debug("Re-completion: Best puzzle ({} clues) above target max ({}), attempting re-completion strategy",
                      best_clue_count, max_clues);
        runRecompletionPhase(best_puzzle, best_clue_count, positions, settings.ensure_unique, min_clues, max_clues,
                             num_orderings, rng);
        spdlog::debug("Re-completion complete: Best puzzle has {} clues (target: {}-{})", best_clue_count, min_clues,
                      max_clues);
    }

    spdlog::debug("Final best puzzle: {} clues (target: {}-{})", best_clue_count, min_clues, max_clues);
    return best_puzzle;
}

// Phase 1: Iterative Deepening Implementation

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — iterative clue removal with uniqueness checking; branching is inherent to puzzle generation
std::optional<BoardData> PuzzleGenerator::removeCluesToTarget(const BoardData& solution, int target_clues,
                                                              int max_attempts, std::mt19937& rng) const {
    // Early exit: if target equals current clues, return solution
    int current_clues = countClues(solution);
    if (current_clues == target_clues) {
        return solution;
    }

    // Early exit: if max_attempts is zero, return empty board
    if (max_attempts == 0) {
        return createEmptyBoard();
    }

    spdlog::debug("removeCluesToTarget: Attempting to reach {} clues (from {}) in {} attempts", target_clues,
                  current_clues, max_attempts);

    int best_clue_count = current_clues;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        auto puzzle = solution;

        // Create shuffled list of all positions
        std::vector<std::pair<size_t, size_t>> all_positions;
        all_positions.reserve(BOARD_SIZE * BOARD_SIZE);

        forEachCell([&](size_t row, size_t col) { all_positions.emplace_back(row, col); });

        std::shuffle(all_positions.begin(), all_positions.end(), rng);

        // Greedy removal with uniqueness checking
        int clues_remaining = countClues(puzzle);
        int restorations = 0;

        for (const auto& [row, col] : all_positions) {
            if (clues_remaining <= target_clues) {
                break;  // Reached target
            }

            if (puzzle[row][col] == 0) {
                continue;  // Already empty
            }

            int original_value = puzzle[row][col];
            puzzle[row][col] = 0;
            clues_remaining--;  // Optimistically decrement

            // Only check uniqueness when within buffer zone of target
            // This allows aggressive removal early on while maintaining uniqueness near target
            constexpr int UNIQUENESS_CHECK_BUFFER = 15;
            bool near_target = (clues_remaining - target_clues) <= UNIQUENESS_CHECK_BUFFER;

            if (near_target && !hasUniqueSolution(puzzle)) {
                // Restore clue - this removal breaks uniqueness
                puzzle[row][col] = original_value;
                clues_remaining++;  // Undo the optimistic decrement
                restorations++;
            } else {
                // Success: reached exact target
                if (clues_remaining == target_clues) {
                    // Final uniqueness verification
                    if (hasUniqueSolution(puzzle)) {
                        spdlog::debug("Attempt {}/{}: SUCCESS - Reached target {} clues (restorations: {})",
                                      attempt + 1, max_attempts, target_clues, restorations);
                        return puzzle;
                    }
                    // Target reached but not unique, restore and continue
                    puzzle[row][col] = original_value;
                    clues_remaining++;
                    restorations++;
                }
            }
        }

        // Check final result
        int final_clues = countClues(puzzle);
        if (final_clues == target_clues && hasUniqueSolution(puzzle)) {
            spdlog::debug("Attempt {}/{}: SUCCESS - Reached target {} clues", attempt + 1, max_attempts, target_clues);
            return puzzle;
        }

        spdlog::trace("Attempt {}/{}: Final {} clues (target: {}, restorations: {})", attempt + 1, max_attempts,
                      final_clues, target_clues, restorations);
    }

    // Failed to reach exact target
    spdlog::debug("removeCluesToTarget: Failed to reach {} clues after {} attempts (best: {})", target_clues,
                  max_attempts, best_clue_count);
    return std::nullopt;
}

std::optional<BoardData> PuzzleGenerator::removeCluesToCreatePuzzleIterative(const BoardData& solution,
                                                                             const GenerationSettings& settings,
                                                                             std::mt19937& rng) const {
    // All difficulties use standard greedy algorithm with multiple orderings + re-completion.
    // Iterative deepening was removed: it optimized for minimum clue count, which does NOT
    // correlate with difficulty. A 17-clue puzzle solvable by naked/hidden singles is Easy,
    // not Expert. Actual difficulty validation (required solving techniques) is handled by
    // the rating check in generatePuzzle().
    return removeCluesToCreatePuzzle(solution, settings, rng);
}

// ============================================================================
// Phase 2: Intelligent Clue Dropping (for improved re-completion strategy)
// ============================================================================

std::vector<ClueAnalysis> PuzzleGenerator::analyzeClueConstraints(const BoardData& board) {
    std::vector<ClueAnalysis> analysis;
    analysis.reserve(BOARD_SIZE * BOARD_SIZE);  // Reserve max possible

    // Iterate over all cells
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            int value = board[row][col];

            // Skip empty cells (only analyze clues)
            if (value == 0) {
                continue;
            }

            // Count how many alternative values could be placed here
            int alternative_count = 0;

            // Try all values 1-9
            for (int test_value = MIN_VALUE; test_value <= MAX_VALUE; ++test_value) {
                // Skip the current value (we're counting alternatives)
                if (test_value == value) {
                    continue;
                }

                // Check if this value would be valid at this position
                Position pos{.row = row, .col = col};
                if (!GameValidator::hasConflict(board, pos, test_value)) {
                    alternative_count++;
                }
            }

            // Compute constraint score: higher score = more constrained (fewer alternatives)
            int constraint_score = MAX_VALUE - alternative_count;

            // Add to analysis
            analysis.push_back(ClueAnalysis{.pos = Position{.row = row, .col = col},
                                            .value = value,
                                            .alternative_count = alternative_count,
                                            .constraint_score = constraint_score});
        }
    }

    // Sort by constraint_score descending (most constrained first)
    std::ranges::sort(analysis, [](const ClueAnalysis& lhs, const ClueAnalysis& rhs) {
        return lhs.constraint_score > rhs.constraint_score;
    });

    spdlog::debug("analyzeClueConstraints: Analyzed {} clues", analysis.size());

    return analysis;
}

std::vector<Position> PuzzleGenerator::selectCluesForDropping(const BoardData& board, int num_clues,
                                                              std::mt19937& rng) {
    // Handle edge cases
    if (num_clues <= 0) {
        return {};
    }

    // Analyze all clues to get constraint scores
    auto analysis = analyzeClueConstraints(board);

    // Cap at total number of clues available
    int clues_to_select = std::min(num_clues, static_cast<int>(analysis.size()));

    std::vector<Position> selected;
    selected.reserve(clues_to_select);

    // Strategy: Prioritize highly constrained clues (higher score) for dropping
    // This creates more diverse re-completion paths (better exploration)
    //
    // Use weighted random selection from top candidates to introduce randomness
    // while still preferring highly constrained clues

    // Take top 2*clues_to_select candidates (or all if fewer available)
    int candidate_pool_size = std::min(clues_to_select * 2, static_cast<int>(analysis.size()));

    // Create a pool of candidates from the most constrained clues
    std::vector<ClueAnalysis> candidate_pool;
    candidate_pool.reserve(candidate_pool_size);

    for (int i = 0; i < candidate_pool_size; ++i) {
        candidate_pool.push_back(analysis[i]);
    }

    // Shuffle candidates to introduce randomness
    std::shuffle(candidate_pool.begin(), candidate_pool.end(), rng);

    // Select the requested number
    for (int i = 0; std::cmp_less(i, clues_to_select) && std::cmp_less(i, candidate_pool.size()); ++i) {
        selected.push_back(candidate_pool[i].pos);
    }

    spdlog::debug("selectCluesForDropping: Selected {} clues from {} candidates (requested: {})", selected.size(),
                  candidate_pool_size, num_clues);

    return selected;
}

bool PuzzleGenerator::solvePuzzleBacktrack(BoardData& board) const {
    // Use unified BacktrackingSolver with sequential strategy (deterministic)
    auto validator_ptr = std::make_shared<GameValidator>(validator_);
    BacktrackingSolver solver(validator_ptr);
    return solver.solve(board, ValueSelectionStrategy::Sequential);
}

bool PuzzleGenerator::fillBoardRecursively(BoardData& board, std::mt19937& rng) const {
    // Use unified BacktrackingSolver with randomized strategy (for puzzle generation)
    auto validator_ptr = std::make_shared<GameValidator>(validator_);
    BacktrackingSolver solver(validator_ptr);
    return solver.solve(board, ValueSelectionStrategy::Randomized, &rng);
}

std::pair<int, int> PuzzleGenerator::getClueRange(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return {EASY_MIN_CLUES, EASY_MAX_CLUES};
        case Difficulty::Medium:
            return {MEDIUM_MIN_CLUES, MEDIUM_MAX_CLUES};
        case Difficulty::Hard:
            return {HARD_MIN_CLUES, HARD_MAX_CLUES};
        case Difficulty::Expert:
            return {EXPERT_MIN_CLUES, EXPERT_MAX_CLUES};
        case Difficulty::Master:
            return {MASTER_MIN_CLUES, MASTER_MAX_CLUES};
        default:
            return {MEDIUM_MIN_CLUES, MEDIUM_MAX_CLUES};  // Default to medium
    }
}

std::optional<Position> PuzzleGenerator::findEmptyPosition(const BoardData& board, bool use_mcv_heuristic) const {
    if (!use_mcv_heuristic) {
        // Sequential scan: Returns first empty cell (backward compatible)
        std::optional<Position> result = std::nullopt;
        anyCell([&](size_t row, size_t col) {
            if (board[row][col] == 0) {
                result = Position{.row = row, .col = col};
                return true;  // Found empty cell, stop searching
            }
            return false;  // Continue searching
        });
        return result;
    }

    // MCV (Most Constrained Variable) heuristic:
    // Select the empty cell with the fewest legal values
    // This reduces branching factor and causes earlier constraint violations
    std::optional<Position> best_position = std::nullopt;
    size_t min_domain_size = MAX_VALUE + 1;  // 10 (impossible size)

    bool early_return = false;
    forEachCell([&](size_t row, size_t col) {
        if (early_return) {
            return;  // Skip remaining cells after early termination
        }
        if (board[row][col] == 0) {
            Position pos{.row = row, .col = col};
            auto possible = validator_.getPossibleValues(board, pos);
            size_t domain_size = possible.size();

            // Early termination: domain size = 1 is optimal (forced move)
            if (domain_size == 1) {
                best_position = pos;
                early_return = true;
                return;
            }

            // Dead-end: domain size = 0 means cell is unsolvable
            // Return immediately to trigger early backtracking
            if (domain_size == 0) {
                best_position = pos;
                early_return = true;
                return;
            }

            if (domain_size < min_domain_size) {
                min_domain_size = domain_size;
                best_position = pos;
            }
        }
    });

    return best_position;
}

std::optional<Position> PuzzleGenerator::findEmptyPosition(const Board& board, bool use_mcv_heuristic) const {
    if (!use_mcv_heuristic) {
        // Sequential scan using flat iteration (more efficient than nested loops)
        for (size_t i = 0; i < TOTAL_CELLS; ++i) {
            if (board.cells[i] == 0) {
                return Position{.row = i / BOARD_SIZE, .col = i % BOARD_SIZE};
            }
        }
        return std::nullopt;
    }

    // MCV heuristic requires validator API (vector<vector<int>>), delegate
    auto vec = board.toBoardData();
    return findEmptyPosition(vec, true);
}

std::vector<int> PuzzleGenerator::getShuffledNumbers(std::mt19937& rng) {
    std::vector<int> numbers;
    numbers.reserve(MAX_VALUE - MIN_VALUE + 1);
    for (int i = MIN_VALUE; i <= MAX_VALUE; ++i) {
        numbers.push_back(i);
    }
    std::shuffle(numbers.begin(), numbers.end(), rng);
    return numbers;
}

BoardData PuzzleGenerator::createEmptyBoard() {
    return BoardData{};
}

}  // namespace sudoku::core
