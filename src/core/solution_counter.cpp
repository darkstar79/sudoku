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

#include "solution_counter.h"

#include "constraint_state.h"
#include "core/board.h"
#include "core/constants.h"
#include "core/i_puzzle_generator.h"
#include "cpu_features.h"
#include "i_game_validator.h"
#include "simd_constraint_state.h"

#include <chrono>
#include <compare>
#include <cstring>
#include <optional>
#include <random>
#include <tuple>
#include <utility>

#include <bit>
#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::core {

namespace {
// Default timeout for solution counting (30 seconds)
constexpr int DEFAULT_SOLUTION_TIMEOUT_MS = 30000;
}  // namespace

SolutionCounter::SolutionCounter() {
    initializeZobristTable();
}

bool SolutionCounter::hasUniqueSolution(const BoardData& board) const {
    return countSolutions(board, 2) == 1;
}

SolverPath SolutionCounter::resolveEffectivePath() const {
    SolverPath path = active_solver_path_;
    if (path == SolverPath::Auto) {
        if (hasAvx512()) {
            path = SolverPath::AVX512;
        } else if (hasAvx2()) {
            path = SolverPath::AVX2;
        } else {
            path = SolverPath::Scalar;
        }
    }
    // Fall back if requested path is unavailable
    if (!isSolverPathAvailable(path)) {
        if (hasAvx2()) {
            path = SolverPath::AVX2;
        } else {
            path = SolverPath::Scalar;
        }
    }
    return path;
}

// CPD-OFF — different timeout semantics and return-on-timeout policies
int SolutionCounter::countSolutions(const BoardData& board, int max_solutions) const {
    // Note: Cache is now cleared per-attempt in generatePuzzle(), not per-call here
    // This allows cache accumulation across multiple countSolutions() calls within
    // a single generation attempt, improving performance when checking uniqueness
    // after each clue removal.

    // Convert to Board for hot path: 96-byte stack copy instead of 9 heap allocations
    auto board_copy = Board::fromBoardData(board);
    int count = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(DEFAULT_SOLUTION_TIMEOUT_MS);
    bool timed_out = false;

    // Compute initial Zobrist hash once; passed incrementally through recursion
    uint64_t hash = computeBoardHash(board_copy);
    uint32_t recursion_count = 0;

    SolverPath effective_path = resolveEffectivePath();

    switch (effective_path) {
        case SolverPath::AVX512:
        case SolverPath::AVX2: {
            SIMDConstraintState simd_state;
            if (effective_path == SolverPath::AVX512) {
                simd_state.setSimdLevel(SolverPath::AVX512);
            }
            simd_state.initFromBoard(board_copy);
            countSolutionsHelperSIMD(board_copy, simd_state, count, max_solutions, start_time, timeout, timed_out, hash,
                                     recursion_count, SIMDConstraintState::ALL_REGIONS_DIRTY);
            break;
        }
        case SolverPath::Scalar:
        case SolverPath::Auto:
        default: {
            ConstraintState state(board_copy);
            countSolutionsHelper(board_copy, state, count, max_solutions, start_time, timeout, timed_out, hash,
                                 recursion_count);
            break;
        }
    }

    // If timed out, assume unique (optimistic approach for normal puzzles)
    // Most timeouts happen on Easy/Medium puzzles that actually ARE unique
    if (timed_out) {
        return 1;  // Return 1 to assume unique on timeout
    }
    return count;
}

int SolutionCounter::countSolutionsWithTimeout(const BoardData& board, int max_solutions,
                                               std::chrono::milliseconds timeout) const {
    // Note: Cache is now cleared per-attempt in generatePuzzle(), not per-call here
    // This allows cache accumulation across multiple countSolutions() calls within
    // a single generation attempt, improving performance when checking uniqueness
    // after each clue removal.

    // Convert to Board for hot path: 96-byte stack copy instead of 9 heap allocations
    auto board_copy = Board::fromBoardData(board);
    int count = 0;
    auto start_time = std::chrono::steady_clock::now();
    bool timed_out = false;

    // Compute initial Zobrist hash once; passed incrementally through recursion
    uint64_t hash = computeBoardHash(board_copy);
    uint32_t recursion_count = 0;

    SolverPath effective_path = resolveEffectivePath();

    switch (effective_path) {
        case SolverPath::AVX512:
        case SolverPath::AVX2: {
            SIMDConstraintState simd_state;
            if (effective_path == SolverPath::AVX512) {
                simd_state.setSimdLevel(SolverPath::AVX512);
            }
            simd_state.initFromBoard(board_copy);
            countSolutionsHelperSIMD(board_copy, simd_state, count, max_solutions, start_time, timeout, timed_out, hash,
                                     recursion_count, SIMDConstraintState::ALL_REGIONS_DIRTY);
            break;
        }
        case SolverPath::Scalar:
        case SolverPath::Auto:
        default: {
            ConstraintState state(board_copy);
            countSolutionsHelper(board_copy, state, count, max_solutions, start_time, timeout, timed_out, hash,
                                 recursion_count);
            break;
        }
    }

    // If timed out, assume non-unique (conservative approach)
    if (timed_out) {
        return max_solutions;  // Return max to indicate non-unique
    }
    return count;
}
// CPD-ON

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — performance-critical recursive backtracking hot path; monolithic for inlining; refactoring adds call overhead on inner loop
void SolutionCounter::countSolutionsHelper(Board& board, ConstraintState& state, int& count, int max_solutions,
                                           const std::chrono::steady_clock::time_point& start_time,
                                           const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                                           uint32_t& recursion_count) const {
    // Sample timeout check every 1024 iterations to avoid vDSO overhead (was 9% of profile)
    if ((++recursion_count & 0x3FF) == 0) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        if (elapsed > timeout) {
            timed_out = true;
            return;
        }
    }

    if (count >= max_solutions) {
        return;  // Early termination - found enough solutions
    }

    // Memoization cache lookup (O(1) per recursion via incremental Zobrist hash)
    const int* cached = solution_count_cache_.find(hash);
    if (cached != nullptr) {
        count += *cached;
        return;
    }

    // Hidden singles detection: find value that can only go in one position within a region
    auto hidden_single = state.findHiddenSingle(board);
    if (hidden_single.has_value()) {
        const auto& [pos, value] = hidden_single.value();

        board[pos.row][pos.col] = static_cast<int8_t>(value);
        state.placeValue(pos.row, pos.col, value);

        int count_before = count;
        uint64_t hs_hash = hash ^ zobrist_table_[pos.row][pos.col][0] ^ zobrist_table_[pos.row][pos.col][value];
        countSolutionsHelper(board, state, count, max_solutions, start_time, timeout, timed_out, hs_hash,
                             recursion_count);

        if (timed_out) {
            board[pos.row][pos.col] = 0;
            state.removeValue(pos.row, pos.col, value);
            return;
        }

        board[pos.row][pos.col] = 0;
        state.removeValue(pos.row, pos.col, value);

        // Only cache if we fully explored the subtree (not truncated by max_solutions)
        if (count < max_solutions) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // MCV heuristic: select cell with fewest legal values for optimal pruning
    // Uses ConstraintState::countPossibleValues() — O(1) via POPCNT, zero heap allocations
    size_t best_row = 0;
    size_t best_col = 0;
    int min_domain = MAX_VALUE + 1;
    bool found_empty = false;

    for (size_t i = 0; i < TOTAL_CELLS; ++i) {
        if (board.cells[i] == 0) {
            size_t row = i / BOARD_SIZE;
            size_t col = i % BOARD_SIZE;
            int domain = state.countPossibleValues(row, col);

            if (domain <= 1) {
                // domain==1: naked single (forced move), domain==0: dead-end
                // Either way, this is the optimal pick — stop searching
                best_row = row;
                best_col = col;
                min_domain = domain;
                found_empty = true;
                break;
            }

            if (domain < min_domain) {
                min_domain = domain;
                best_row = row;
                best_col = col;
                found_empty = true;
            }
        }
    }

    if (!found_empty) {
        ++count;
        solution_count_cache_.insert(hash, 1);
        return;
    }

    Position pos{.row = best_row, .col = best_col};

    // Naked singles: cell with exactly 1 candidate is a forced move
    if (min_domain == 1) {
        uint16_t mask = state.getPossibleValuesMask(pos.row, pos.col);
        int forced_value = std::countr_zero(static_cast<unsigned>(mask));  // Bits 1-9 represent digits 1-9

        board[pos.row][pos.col] = static_cast<int8_t>(forced_value);
        state.placeValue(pos.row, pos.col, forced_value);
        int count_before = count;

        uint64_t ns_hash = hash ^ zobrist_table_[pos.row][pos.col][0] ^ zobrist_table_[pos.row][pos.col][forced_value];
        countSolutionsHelper(board, state, count, max_solutions, start_time, timeout, timed_out, ns_hash,
                             recursion_count);

        if (timed_out) {
            board[pos.row][pos.col] = 0;
            state.removeValue(pos.row, pos.col, forced_value);
            return;
        }

        board[pos.row][pos.col] = 0;
        state.removeValue(pos.row, pos.col, forced_value);

        // Only cache if we fully explored the subtree (not truncated by max_solutions)
        if (count < max_solutions) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // Try all valid values using O(1) ConstraintState validation
    int solutions_from_this_state = 0;

    for (int num = MIN_VALUE; num <= MAX_VALUE; ++num) {
        if (state.isAllowed(pos.row, pos.col, num)) {
            board[pos.row][pos.col] = static_cast<int8_t>(num);
            state.placeValue(pos.row, pos.col, num);
            int count_before = count;

            uint64_t new_hash = hash ^ zobrist_table_[pos.row][pos.col][0] ^ zobrist_table_[pos.row][pos.col][num];
            countSolutionsHelper(board, state, count, max_solutions, start_time, timeout, timed_out, new_hash,
                                 recursion_count);

            if (timed_out) {
                board[pos.row][pos.col] = 0;
                state.removeValue(pos.row, pos.col, num);
                return;
            }

            solutions_from_this_state += (count - count_before);
            board[pos.row][pos.col] = 0;
            state.removeValue(pos.row, pos.col, num);

            if (count >= max_solutions) {
                break;
            }
        }
    }

    // Only cache if we fully explored all branches (not truncated by max_solutions)
    if (count < max_solutions) {
        solution_count_cache_.insert(hash, solutions_from_this_state);
    }
}

void SolutionCounter::initializeZobristTable() {
    // Use fixed seed for Zobrist table (must be consistent across runs)
    // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp) - Deterministic seed required for Zobrist hashing
    std::mt19937_64 rng(0x123456789ABCDEF0ULL);
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

    // Generate random 64-bit values for each (row, col, value) combination
    // Index 0 represents empty cell, indices 1-9 represent values 1-9
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            for (int value = 0; value <= MAX_VALUE; ++value) {
                zobrist_table_[row][col][value] = dist(rng);
            }
        }
    }
}

uint64_t SolutionCounter::computeBoardHash(const Board& board) const {
    uint64_t hash = 0;

    // XOR hash values for each filled cell (flat iteration over Board)
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            auto value = static_cast<size_t>(static_cast<unsigned char>(board[row][col]));
            // XOR with the zobrist value for this (row, col, value) combination
            hash ^= zobrist_table_[row][col][value];
        }
    }

    return hash;
}

void SolutionCounter::clearCache() const {
    solution_count_cache_.clear();
}

// ============================================================================
// SIMD-OPTIMIZED SOLUTION COUNTING (AVX2)
// ============================================================================
// This implementation uses SIMDConstraintState for faster:
// - MRV heuristic: SIMD popcount finds cell with min candidates in 6 iterations
// - Candidate enumeration: Bitmask iteration with CTZ instead of 1-9 loop
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — SIMD hot path; save/restore of 20-peer candidate arrays requires explicit inline code; same structure as scalar version
void SolutionCounter::countSolutionsHelperSIMD(Board& board, SIMDConstraintState& state, int& count, int max_solutions,
                                               const std::chrono::steady_clock::time_point& start_time,
                                               const std::chrono::milliseconds& timeout, bool& timed_out, uint64_t hash,
                                               uint32_t& recursion_count, uint32_t dirty_regions) const {
    // Sample timeout check every 1024 iterations to avoid vDSO overhead (was 9% of profile)
    if ((++recursion_count & 0x3FF) == 0) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        if (elapsed > timeout) {
            timed_out = true;
            return;
        }
    }

    if (count >= max_solutions) {
        return;
    }

    // Memoization cache lookup (O(1) per recursion via incremental Zobrist hash)
    const int* cached = solution_count_cache_.find(hash);
    if (cached != nullptr) {
        count += *cached;
        return;
    }

    // OPTIMIZATION: Hidden singles detection (CRITICAL for performance)
    // Fast path: scan only dirty regions (3 vs 27) for hidden singles created by the last placement.
    // If nothing found in dirty regions, fall back to full scan for pre-existing hidden singles
    // that the parent's early-return scan didn't reach (e.g., in cols/boxes after row hit).
    auto [hs_cell, hs_digit] = state.findHiddenSingle(board, dirty_regions);
    if (hs_cell < 0 && dirty_regions != SIMDConstraintState::ALL_REGIONS_DIRTY) {
        std::tie(hs_cell, hs_digit) = state.findHiddenSingle(board);
    }
    if (hs_cell >= 0) {
        auto hs_cell_idx = static_cast<size_t>(hs_cell);
        size_t hs_row = SIMDConstraintState::cellRow(hs_cell_idx);
        size_t hs_col = SIMDConstraintState::cellCol(hs_cell_idx);

        // Save full candidates array for backtracking (bulk memcpy: ~6 SIMD stores vs 20 scattered loads)
        alignas(SIMD_ALIGNMENT) std::array<uint16_t, SIMD_PADDED_CELLS>
            hs_saved_candidates;  // NOLINT(cppcoreguidelines-pro-type-member-init) — immediately memcpy'd
        std::memcpy(hs_saved_candidates.data(), state.candidates.data(), sizeof(state.candidates));

        // Save region tracking state
        uint16_t saved_row_used = state.row_used_[hs_row];
        uint16_t saved_col_used = state.col_used_[hs_col];
        size_t hs_box = SIMDConstraintState::getBoxIndex(hs_row, hs_col);
        uint16_t saved_box_used = state.box_used_[hs_box];

        // Place the hidden single (forced move - no branching)
        board[hs_row][hs_col] = static_cast<int8_t>(hs_digit);
        state.place(hs_cell_idx, hs_digit);

        int count_before = count;
        // Incremental hash: XOR out empty cell, XOR in placed digit
        uint64_t hs_hash =
            hash ^ zobrist_table_[hs_row][hs_col][0] ^ zobrist_table_[hs_row][hs_col][static_cast<size_t>(hs_digit)];
        // Dirty regions: only the row, col, box of the placed cell
        uint32_t hs_dirty = (1U << hs_row) | (1U << (9 + hs_col)) | (1U << (18 + hs_box));
        countSolutionsHelperSIMD(board, state, count, max_solutions, start_time, timeout, timed_out, hs_hash,
                                 recursion_count, hs_dirty);

        // Backtrack: restore board
        board[hs_row][hs_col] = 0;

        // Backtrack: restore full candidates array
        std::memcpy(state.candidates.data(), hs_saved_candidates.data(), sizeof(state.candidates));

        // Backtrack: restore region tracking
        state.row_used_[hs_row] = saved_row_used;
        state.col_used_[hs_col] = saved_col_used;
        state.box_used_[hs_box] = saved_box_used;

        // Only cache if we fully explored the subtree (not truncated by max_solutions or timeout)
        if (count < max_solutions && !timed_out) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // OPTIMIZATION: Naked singles propagation
    // Find cells with exactly 1 candidate and place them (no branching choice)
    int naked_single = state.findNakedSingle(board);
    if (naked_single >= 0) {
        auto ns_cell = static_cast<size_t>(naked_single);
        size_t ns_row = SIMDConstraintState::cellRow(ns_cell);
        size_t ns_col = SIMDConstraintState::cellCol(ns_cell);
        int ns_digit = state.getSolvedDigit(ns_cell);

        if (ns_digit == 0) {
            // Contradiction: cell has 0 candidates
            solution_count_cache_.insert(hash, 0);
            return;
        }

        // Save full candidates array for backtracking (bulk memcpy)
        alignas(SIMD_ALIGNMENT) std::array<uint16_t, SIMD_PADDED_CELLS>
            ns_saved_candidates;  // NOLINT(cppcoreguidelines-pro-type-member-init) — immediately memcpy'd
        std::memcpy(ns_saved_candidates.data(), state.candidates.data(), sizeof(state.candidates));

        // Save region tracking state
        uint16_t saved_row_used = state.row_used_[ns_row];
        uint16_t saved_col_used = state.col_used_[ns_col];
        size_t ns_box = SIMDConstraintState::getBoxIndex(ns_row, ns_col);
        uint16_t saved_box_used = state.box_used_[ns_box];

        // Place the naked single (forced move)
        board[ns_row][ns_col] = static_cast<int8_t>(ns_digit);
        state.place(ns_cell, ns_digit);

        int count_before = count;
        // Incremental hash: XOR out empty cell, XOR in placed digit
        uint64_t ns_hash =
            hash ^ zobrist_table_[ns_row][ns_col][0] ^ zobrist_table_[ns_row][ns_col][static_cast<size_t>(ns_digit)];
        // Dirty regions: only the row, col, box of the placed cell
        uint32_t ns_dirty = (1U << ns_row) | (1U << (9 + ns_col)) | (1U << (18 + ns_box));
        countSolutionsHelperSIMD(board, state, count, max_solutions, start_time, timeout, timed_out, ns_hash,
                                 recursion_count, ns_dirty);

        // Backtrack: restore board
        board[ns_row][ns_col] = 0;

        // Backtrack: restore full candidates array
        std::memcpy(state.candidates.data(), ns_saved_candidates.data(), sizeof(state.candidates));

        // Backtrack: restore region tracking
        state.row_used_[ns_row] = saved_row_used;
        state.col_used_[ns_col] = saved_col_used;
        state.box_used_[ns_box] = saved_box_used;

        // Only cache if we fully explored the subtree (not truncated by max_solutions or timeout)
        if (count < max_solutions && !timed_out) {
            solution_count_cache_.insert(hash, count - count_before);
        }
        return;
    }

    // SIMD MRV heuristic: Find cell with minimum candidates > 1
    int cell = state.findMRVCell();

    // No unsolved cells with > 1 candidates - all cells are solved!
    if (cell < 0) {
        // All cells are either filled on board OR have exactly 0 candidates (contradiction)
        // Since we handled naked singles above, findMRVCell returning -1 means
        // all unsolved cells have 0 candidates (contradiction) or board is complete
        bool complete = true;
        for (size_t row = 0; row < BOARD_SIZE && complete; ++row) {
            for (size_t col = 0; col < BOARD_SIZE && complete; ++col) {
                if (board[row][col] == 0) {
                    complete = false;
                }
            }
        }
        if (complete) {
            ++count;
            solution_count_cache_.insert(hash, 1);
        } else {
            // Contradiction - empty cell with no candidates
            solution_count_cache_.insert(hash, 0);
        }
        return;
    }

    // Check for contradiction at selected cell (no candidates)
    if (state.isContradiction(static_cast<size_t>(cell))) {
        solution_count_cache_.insert(hash, 0);
        return;
    }

    size_t row = SIMDConstraintState::cellRow(static_cast<size_t>(cell));
    size_t col = SIMDConstraintState::cellCol(static_cast<size_t>(cell));

    // Get candidate bitmask and iterate set bits (faster than 1-9 loop)
    uint16_t candidates = state.getCandidates(static_cast<size_t>(cell));

    // Save full candidates array for backtracking (bulk memcpy)
    alignas(SIMD_ALIGNMENT) std::array<uint16_t, SIMD_PADDED_CELLS>
        saved_candidates;  // NOLINT(cppcoreguidelines-pro-type-member-init) — immediately memcpy'd
    std::memcpy(saved_candidates.data(), state.candidates.data(), sizeof(state.candidates));

    // Save region tracking state
    uint16_t saved_row_used = state.row_used_[row];
    uint16_t saved_col_used = state.col_used_[col];
    size_t box = SIMDConstraintState::getBoxIndex(row, col);
    uint16_t saved_box_used = state.box_used_[box];

    int solutions_from_this_state = 0;

    // Iterate only valid candidates using CTZ (count trailing zeros)
    while (candidates != 0 && count < max_solutions) {
        // Extract lowest set bit (digit - 1)
        int bit_pos = std::countr_zero(static_cast<unsigned>(candidates));
        int digit = bit_pos + 1;

        // Clear lowest set bit for next iteration
        candidates &= candidates - 1;

        // Place digit
        board[row][col] = static_cast<int8_t>(digit);
        state.place(static_cast<size_t>(cell), digit);

        int count_before_recurse = count;

        // Recurse with incremental hash: XOR out empty cell, XOR in placed digit
        uint64_t new_hash = hash ^ zobrist_table_[row][col][0] ^ zobrist_table_[row][col][static_cast<size_t>(digit)];
        // Dirty regions: only the row, col, box of the placed cell
        uint32_t branch_dirty = (1U << row) | (1U << (9 + col)) | (1U << (18 + box));
        countSolutionsHelperSIMD(board, state, count, max_solutions, start_time, timeout, timed_out, new_hash,
                                 recursion_count, branch_dirty);

        solutions_from_this_state += (count - count_before_recurse);

        // Backtrack: restore board
        board[row][col] = 0;

        // Backtrack: restore full candidates array
        std::memcpy(state.candidates.data(), saved_candidates.data(), sizeof(state.candidates));

        // Backtrack: restore region tracking
        state.row_used_[row] = saved_row_used;
        state.col_used_[col] = saved_col_used;
        state.box_used_[box] = saved_box_used;

        if (timed_out) {
            return;
        }
    }

    // Only cache if we fully explored all branches (not truncated by max_solutions or timeout)
    if (count < max_solutions && !timed_out) {
        solution_count_cache_.insert(hash, solutions_from_this_state);
    }
}

bool SolutionCounter::hasContradiction(const BoardData& board) {
    ConstraintState state(board);

    // Check each empty cell for empty domain (no legal values)
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0) {
                // Count possible values using ConstraintState
                int possible_count = state.countPossibleValues(row, col);

                // If no legal values, contradiction exists
                if (possible_count == 0) {
                    return true;
                }
            }
        }
    }

    return false;  // No contradiction found
}

std::expected<BoardData, GenerationError>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — constraint propagation loop with naked/hidden single detection; algorithmically coupled, cannot split without losing context
SolutionCounter::propagateConstraintsScalar(const BoardData& board) {
    auto propagated = board;
    ConstraintState state(propagated);

    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 100;  // Prevent infinite loops

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try to find and place naked singles
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (propagated[row][col] == 0) {
                    int possible_count = state.countPossibleValues(row, col);

                    // Naked single: exactly one possible value
                    if (possible_count == 1) {
                        // Find which value it is
                        uint16_t mask = state.getPossibleValuesMask(row, col);
                        int value = 0;
                        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                            if ((mask & valueToBit(v)) != 0) {
                                value = v;
                                break;
                            }
                        }

                        propagated[row][col] = value;
                        state.placeValue(row, col, value);
                        changed = true;
                        spdlog::debug("Propagation: Naked single found at ({}, {}) = {}", row, col, value);
                    } else if (possible_count == 0) {
                        // Contradiction: no legal values
                        spdlog::debug("Propagation: Contradiction at ({}, {}) - empty domain", row, col);
                        return std::unexpected(GenerationError::NoUniqueSolution);
                    }
                }
            }
        }

        // Try to find and place hidden singles (only if no naked singles found)
        if (!changed) {
            auto hidden_opt = state.findHiddenSingle(propagated);
            if (hidden_opt.has_value()) {
                const auto& [pos, value] = hidden_opt.value();
                propagated[pos.row][pos.col] = value;
                state.placeValue(pos.row, pos.col, value);
                changed = true;
                spdlog::debug("Propagation: Hidden single found at ({}, {}) = {}", pos.row, pos.col, value);
            }
        }
    }

    if (iteration_count >= MAX_ITERATIONS) {
        spdlog::warn("Propagation: Max iterations ({}) reached", MAX_ITERATIONS);
    } else {
        spdlog::debug("Propagation: Fixed point reached after {} iterations", iteration_count);
    }

    return propagated;
}

std::expected<BoardData, GenerationError> SolutionCounter::propagateConstraintsSIMD(const BoardData& board) {
    auto propagated = board;
    SIMDConstraintState state;
    state.initFromBoard(propagated);

    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 100;

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try to find and place naked singles using SIMD
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (propagated[row][col] == 0) {
                    size_t cell_idx = (row * BOARD_SIZE) + col;
                    int candidate_count = state.countCandidates(cell_idx);

                    if (candidate_count == 1) {
                        // Naked single found
                        int value = state.getSolvedDigit(cell_idx);
                        propagated[row][col] = value;
                        state.place(cell_idx, value);
                        changed = true;
                        spdlog::debug("Propagation (SIMD): Naked single at ({}, {}) = {}", row, col, value);
                    } else if (candidate_count == 0) {
                        // Contradiction
                        spdlog::debug("Propagation (SIMD): Contradiction at ({}, {}) - empty domain", row, col);
                        return std::unexpected(GenerationError::NoUniqueSolution);
                    }
                }
            }
        }

        // Try hidden singles with SIMD (returns std::pair<int, int>, -1 means not found)
        if (!changed) {
            auto [cell_idx, value] = state.findHiddenSingle(propagated);
            if (cell_idx >= 0) {
                size_t row = static_cast<size_t>(cell_idx) / BOARD_SIZE;
                size_t col = static_cast<size_t>(cell_idx) % BOARD_SIZE;
                propagated[row][col] = value;
                state.place(static_cast<size_t>(cell_idx), value);
                changed = true;
                spdlog::debug("Propagation (SIMD): Hidden single at ({}, {}) = {}", row, col, value);
            }
        }
    }

    if (iteration_count >= MAX_ITERATIONS) {
        spdlog::warn("Propagation (SIMD): Max iterations ({}) reached", MAX_ITERATIONS);
    } else {
        spdlog::debug("Propagation (SIMD): Fixed point reached after {} iterations", iteration_count);
    }

    return propagated;
}

std::expected<BoardData, GenerationError> SolutionCounter::propagateConstraints(const BoardData& board) {
    if (hasAvx2()) {
        return propagateConstraintsSIMD(board);
    }
    return propagateConstraintsScalar(board);
}

// ============================================================================
// Phase 3: Solver Integration - Iterative Propagation Helpers
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — iterative propagation loop; naked/hidden single passes are tightly coupled; complexity is inherent to constraint propagation
[[nodiscard]] bool SolutionCounter::applyIterativePropagationScalar(BoardData& board, ConstraintState& state) {
    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 81;  // Maximum possible iterations (9x9 board)

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try naked singles first (most efficient to find)
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == 0) {
                    int possible_count = state.countPossibleValues(row, col);

                    if (possible_count == 1) {
                        // Extract the single value from bitmask
                        uint16_t mask = state.getPossibleValuesMask(row, col);
                        int value = 0;
                        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                            if ((mask & valueToBit(v)) != 0) {
                                value = v;
                                break;
                            }
                        }

                        board[row][col] = value;
                        state.placeValue(row, col, value);
                        changed = true;
                    } else if (possible_count == 0) {
                        // Contradiction detected
                        return false;
                    }
                }
            }
        }

        // Try hidden singles if no naked singles found
        if (!changed) {
            auto hidden_opt = state.findHiddenSingle(board);
            if (hidden_opt.has_value()) {
                const auto& [pos, value] = hidden_opt.value();
                board[pos.row][pos.col] = value;
                state.placeValue(pos.row, pos.col, value);
                changed = true;
            }
        }
    }

    // Check if board is complete
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0) {
                return false;  // Not complete yet
            }
        }
    }

    return true;  // Board is complete!
}

[[nodiscard]] bool SolutionCounter::applyIterativePropagationSIMD(BoardData& board, SIMDConstraintState& state) {
    bool changed = true;
    int iteration_count = 0;
    constexpr int MAX_ITERATIONS = 81;

    while (changed && iteration_count < MAX_ITERATIONS) {
        changed = false;
        iteration_count++;

        // Try hidden singles first (higher priority, more powerful)
        auto [hs_cell, hs_digit] = state.findHiddenSingle(board);
        if (hs_cell >= 0) {
            size_t hs_row = SIMDConstraintState::cellRow(static_cast<size_t>(hs_cell));
            size_t hs_col = SIMDConstraintState::cellCol(static_cast<size_t>(hs_cell));

            board[hs_row][hs_col] = hs_digit;
            state.place(static_cast<size_t>(hs_cell), hs_digit);
            changed = true;
            continue;  // Restart loop to find more forced moves
        }

        // Try naked singles
        int ns_cell = state.findNakedSingle(board);
        if (ns_cell >= 0) {
            size_t ns_row = SIMDConstraintState::cellRow(static_cast<size_t>(ns_cell));
            size_t ns_col = SIMDConstraintState::cellCol(static_cast<size_t>(ns_cell));
            int ns_digit = state.getSolvedDigit(static_cast<size_t>(ns_cell));

            if (ns_digit == 0) {
                // Contradiction: cell has 0 candidates
                return false;
            }

            board[ns_row][ns_col] = ns_digit;
            state.place(static_cast<size_t>(ns_cell), ns_digit);
            changed = true;
            continue;
        }
    }

    // Check if board is complete
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0) {
                return false;  // Not complete yet
            }
        }
    }

    return true;  // Board is complete!
}

}  // namespace sudoku::core
