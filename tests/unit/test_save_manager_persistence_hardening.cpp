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

/// Persistence-hardening pass (#24 Task 9 / CODE_REVIEW_2026-05-25.md §3.4) — the pre-1.0.0 save
/// robustness gate:
///   1. save round-trip fuzz: a randomized SavedGame survives serialize → deserialize value-equal,
///      across difficulties (incl. Master) and both compressed/uncompressed;
///   3. catastrophic-failure recovery: a corrupt save is archived aside, never overwritten.
/// (The format-version migration test (2) lives in test_save_manager_deserialization.cpp next to the
/// version fixtures.)

#include "../../src/core/i_time_provider.h"
#include "../../src/core/save_manager.h"
#include "../helpers/test_utils.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

namespace {

// Build a randomized but structurally-valid SavedGame from a seed. Covers the gameplay-critical
// surface the save format must preserve: both boards, notes, hint flags, progress counters, rating
// provenance, and move history.
SavedGame makeRandomGame(std::mt19937& rng, Difficulty difficulty) {
    std::uniform_int_distribution<int> digit(0, 9);
    std::uniform_int_distribution<int> coin(0, 1);

    SavedGame game;
    game.display_name = "Fuzz";
    game.difficulty = difficulty;
    game.puzzle_seed = rng();
    game.origin = static_cast<PuzzleOrigin>(rng() % 3);

    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            game.original_puzzle[r][c] = digit(rng);
            game.current_state[r][c] = digit(rng);
            for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                if (coin(rng) == 1) {
                    game.notes[r][c].add(v);
                }
            }
            if (coin(rng) == 1) {
                game.hint_revealed_cells.set(r, c, true);
            }
        }
    }

    game.elapsed_time = std::chrono::milliseconds(rng() % 100000);
    game.moves_made = static_cast<int>(rng() % 200);
    game.hints_used = static_cast<int>(rng() % 20);
    game.mistakes = static_cast<int>(rng() % 20);
    game.is_complete = coin(rng) == 1;
    game.puzzle_rating = static_cast<double>(rng() % 110) / 10.0;
    game.puzzle_requires_backtracking = coin(rng) == 1;
    game.rating_model_version = static_cast<int>(rng() % 3);
    game.puzzle_technique_ids = {static_cast<int>(rng() % 40), static_cast<int>(rng() % 40)};

    const int move_count = static_cast<int>(rng() % 10);
    for (int i = 0; i < move_count; ++i) {
        Move move;
        move.position = {.row = rng() % BOARD_SIZE, .col = rng() % BOARD_SIZE};
        move.value = digit(rng);
        move.move_type = static_cast<MoveType>(rng() % 5);
        move.is_note = coin(rng) == 1;
        game.move_history.push_back(move);
    }
    game.current_move_index = move_count > 0 ? static_cast<int>(rng() % move_count) : -1;

    return game;
}

// Assert every field the serializer is responsible for survives the round-trip. save_id /
// last_modified / created_time / display_name are intentionally excluded — saveGame() stamps them.
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — a flat list of REQUIRE field checks; the count, not nesting, drives the score
void requireRoundTripEqual(const SavedGame& original, const SavedGame& loaded) {
    REQUIRE(loaded.original_puzzle == original.original_puzzle);
    REQUIRE(loaded.current_state == original.current_state);
    REQUIRE(loaded.notes == original.notes);
    REQUIRE(loaded.hint_revealed_cells == original.hint_revealed_cells);
    REQUIRE(loaded.difficulty == original.difficulty);
    REQUIRE(loaded.puzzle_seed == original.puzzle_seed);
    REQUIRE(loaded.origin == original.origin);
    REQUIRE(loaded.elapsed_time == original.elapsed_time);
    REQUIRE(loaded.moves_made == original.moves_made);
    REQUIRE(loaded.hints_used == original.hints_used);
    REQUIRE(loaded.mistakes == original.mistakes);
    REQUIRE(loaded.is_complete == original.is_complete);
    REQUIRE(loaded.puzzle_rating == original.puzzle_rating);
    REQUIRE(loaded.puzzle_requires_backtracking == original.puzzle_requires_backtracking);
    REQUIRE(loaded.rating_model_version == original.rating_model_version);
    REQUIRE(loaded.puzzle_technique_ids == original.puzzle_technique_ids);

    REQUIRE(loaded.move_history.size() == original.move_history.size());
    for (size_t i = 0; i < original.move_history.size(); ++i) {
        const auto& expected = original.move_history[i];
        const auto& actual = loaded.move_history[i];
        REQUIRE(actual.position == expected.position);
        REQUIRE(actual.value == expected.value);
        REQUIRE(actual.move_type == expected.move_type);
        REQUIRE(actual.is_note == expected.is_note);
    }
    REQUIRE(loaded.current_move_index == original.current_move_index);
}

}  // namespace

// #24 Task 9.1 — property/fuzz round-trip. A randomized save (board + notes + move history +
// metadata) must survive serialize → deserialize value-equal. The seed list is fixed so any failure
// reproduces; difficulty spans Easy..Master and compression is exercised both ways.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) — Catch2 TEST_CASE macro expansion
TEST_CASE("SaveManager - randomized save survives a serialize/deserialize round-trip",
          "[save_manager][persistence][fuzz]") {
    const Difficulty difficulty =
        GENERATE(Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert, Difficulty::Master);
    const unsigned int seed = GENERATE(1U, 42U, 7777U);
    const bool compress = GENERATE(true, false);

    std::mt19937 rng(seed);
    const SavedGame original = makeRandomGame(rng, difficulty);

    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SaveSettings settings;
    settings.compress = compress;
    settings.include_history = true;

    auto save_result = mgr.saveGame(original, settings);
    REQUIRE(save_result.has_value());

    auto loaded = mgr.loadGame(*save_result);
    REQUIRE(loaded.has_value());

    requireRoundTripEqual(original, *loaded);
}

// #24 Task 9.3 — catastrophic-failure recovery. A deliberately-corrupt (unparseable) save must
// surface a recoverable error AND have its bytes preserved in a `.corrupt-<ts>` archive rather than
// be left where the next save would clobber them. Mirrors StatisticsManager's session-archive idiom.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) — Catch2 TEST_CASE macro expansion
TEST_CASE("SaveManager - corrupt save is archived aside and never overwritten",
          "[save_manager][persistence][recovery]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Unparseable YAML → SerializationError on load.
    const std::string corrupt_bytes = "version: '1.1'\nputrid: {unclosed\n  junk: [1, 2\n";
    const auto save_path = tmp.path() / "corrupt.yaml";
    {
        std::ofstream out(save_path);
        out << corrupt_bytes;
    }

    auto result = mgr.loadGame("corrupt");
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == SaveError::SerializationError);

    // The original path must no longer hold the file (a later save can't clobber it)...
    REQUIRE(!fs::exists(save_path));

    // ...and exactly one archive must hold the original bytes intact.
    std::vector<fs::path> archives;
    for (const auto& entry : fs::directory_iterator(tmp.path())) {
        if (entry.path().filename().string().starts_with("corrupt.yaml.corrupt-")) {
            archives.push_back(entry.path());
        }
    }
    REQUIRE(archives.size() == 1);

    std::ifstream in(archives.front());
    const std::string archived((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(archived == corrupt_bytes);
}
