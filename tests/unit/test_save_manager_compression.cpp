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

#include "../../src/core/save_manager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
namespace fs = std::filesystem;

// RAII helper for temporary test directory management
class TempTestDir {
public:
    TempTestDir()
        : path_(fs::temp_directory_path() /
                ("sudoku_compression_test_" +
                 std::to_string(std::chrono::system_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path_);
    }

    ~TempTestDir() {
        if (fs::exists(path_)) {
            fs::remove_all(path_);
        }
    }

    const fs::path& path() const {
        return path_;
    }

private:
    fs::path path_;
};

// Helper: Create a SavedGame with rich test data
SavedGame createRichTestGame() {
    SavedGame game;

    // Board with mixed values (givens, user entries, empty cells)
    game.current_state = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                          {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                          {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

    game.original_puzzle = game.current_state;

    // Notes on multiple cells
    game.notes = NotesData{};
    game.notes[0][2] = {1, 2, 4};  // Cell (0,2) has notes 1,2,4
    game.notes[1][1] = {2, 4, 7};  // Cell (1,1) has notes 2,4,7
    game.notes[2][0] = {1};        // Cell (2,0) has note 1

    // Hint revealed cells
    game.hint_revealed_cells = HintMaskData{};
    game.hint_revealed_cells.set(0, 3, true);

    // Game metadata
    game.difficulty = Difficulty::Medium;
    game.puzzle_seed = 42;
    game.elapsed_time = std::chrono::milliseconds(123456);
    game.moves_made = 15;
    game.hints_used = 2;
    game.mistakes = 1;
    game.is_complete = false;
    game.created_time = std::chrono::system_clock::now();
    game.last_modified = game.created_time;
    // Note: display_name will be set by SaveManager from SaveSettings.custom_name

    return game;
}

// Helper: Create empty/minimal game
SavedGame createEmptyTestGame() {
    SavedGame game;
    game.current_state = BoardData{};
    game.original_puzzle = game.current_state;
    game.notes = NotesData{};
    game.hint_revealed_cells = HintMaskData{};
    game.difficulty = Difficulty::Easy;
    game.puzzle_seed = 0;
    game.created_time = std::chrono::system_clock::now();
    game.last_modified = game.created_time;
    return game;
}

// Helper: Verify SavedGame equality
bool gamesAreEqual(const SavedGame& a, const SavedGame& b) {
    if (a.current_state != b.current_state) {
        return false;
    }
    if (a.original_puzzle != b.original_puzzle) {
        return false;
    }
    if (a.notes != b.notes) {
        return false;
    }
    if (a.hint_revealed_cells != b.hint_revealed_cells) {
        return false;
    }
    if (a.difficulty != b.difficulty) {
        return false;
    }
    if (a.puzzle_seed != b.puzzle_seed) {
        return false;
    }
    if (a.moves_made != b.moves_made) {
        return false;
    }
    if (a.hints_used != b.hints_used) {
        return false;
    }
    if (a.mistakes != b.mistakes) {
        return false;
    }
    if (a.is_complete != b.is_complete) {
        return false;
    }
    return true;
}

TEST_CASE("Compression round-trip preserves all game data", "[save_manager][compression]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto original_game = createRichTestGame();

    SECTION("Compressed save preserves all data") {
        // Save with compression
        SaveSettings settings;
        settings.compress = true;
        settings.custom_name = "Compressed Round-Trip Test";

        auto save_result = save_manager.saveGame(original_game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;

        // Load the saved game
        auto load_result = save_manager.loadGame(save_id);
        REQUIRE(load_result.has_value());

        auto& loaded_game = *load_result;

        // Verify all data matches
        REQUIRE(gamesAreEqual(original_game, loaded_game));

        // Verify specific fields
        REQUIRE(loaded_game.current_state == original_game.current_state);
        REQUIRE(loaded_game.notes == original_game.notes);
        REQUIRE(loaded_game.elapsed_time == original_game.elapsed_time);
        REQUIRE(loaded_game.difficulty == original_game.difficulty);
    }
}

TEST_CASE("Compression reduces file size", "[save_manager][compression]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto game = createRichTestGame();

    // Save with compression
    SaveSettings compressed_settings;
    compressed_settings.compress = true;
    compressed_settings.custom_name = "Compressed";

    auto compressed_save_result = save_manager.saveGame(game, compressed_settings);
    REQUIRE(compressed_save_result.has_value());

    std::string compressed_id = *compressed_save_result;

    // Save without compression
    SaveSettings uncompressed_settings;
    uncompressed_settings.compress = false;
    uncompressed_settings.custom_name = "Uncompressed";

    auto uncompressed_save_result = save_manager.saveGame(game, uncompressed_settings);
    REQUIRE(uncompressed_save_result.has_value());

    std::string uncompressed_id = *uncompressed_save_result;

    // Measure file sizes
    auto compressed_path = temp_dir.path() / (compressed_id + ".yaml");
    auto uncompressed_path = temp_dir.path() / (uncompressed_id + ".yaml");

    REQUIRE(fs::exists(compressed_path));
    REQUIRE(fs::exists(uncompressed_path));

    auto compressed_size = fs::file_size(compressed_path);
    auto uncompressed_size = fs::file_size(uncompressed_path);

    // Verify compressed is smaller
    REQUIRE(compressed_size < uncompressed_size);

    // Calculate reduction percentage
    double reduction = 100.0 * (1.0 - static_cast<double>(compressed_size) / uncompressed_size);

    // Verify at least 30% reduction (YAML typically compresses well)
    REQUIRE(reduction >= 30.0);
}

TEST_CASE("Can load uncompressed saves (backward compatibility)", "[save_manager][compression]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto original_game = createRichTestGame();

    // Save without compression (old format)
    SaveSettings settings;
    settings.compress = false;
    settings.custom_name = "Backward Compatibility Test";

    auto save_result = save_manager.saveGame(original_game, settings);
    REQUIRE(save_result.has_value());

    std::string save_id = *save_result;

    // Load with new deserializer (should auto-detect uncompressed format)
    auto load_result = save_manager.loadGame(save_id);
    REQUIRE(load_result.has_value());

    auto& loaded_game = *load_result;

    // Verify all data loads correctly
    REQUIRE(gamesAreEqual(original_game, loaded_game));
}

TEST_CASE("Can list and load mixed compressed/uncompressed saves", "[save_manager][compression]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    // Create 3 games
    auto game1 = createRichTestGame();
    auto game2 = createRichTestGame();
    auto game3 = createRichTestGame();

    // Save game1 compressed
    SaveSettings settings1;
    settings1.compress = true;
    settings1.custom_name = "Compressed Game 1";
    auto save1 = save_manager.saveGame(game1, settings1);
    REQUIRE(save1.has_value());

    // Save game2 uncompressed
    SaveSettings settings2;
    settings2.compress = false;
    settings2.custom_name = "Uncompressed Game 2";
    auto save2 = save_manager.saveGame(game2, settings2);
    REQUIRE(save2.has_value());

    // Save game3 compressed
    SaveSettings settings3;
    settings3.compress = true;
    settings3.custom_name = "Compressed Game 3";
    auto save3 = save_manager.saveGame(game3, settings3);
    REQUIRE(save3.has_value());

    // List all saves
    auto list_result = save_manager.listSaves();
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->size() == 3);

    // Load each save individually
    auto load1 = save_manager.loadGame(*save1);
    REQUIRE(load1.has_value());

    auto load2 = save_manager.loadGame(*save2);
    REQUIRE(load2.has_value());

    auto load3 = save_manager.loadGame(*save3);
    REQUIRE(load3.has_value());

    // Verify names
    REQUIRE(load1->display_name == "Compressed Game 1");
    REQUIRE(load2->display_name == "Uncompressed Game 2");
    REQUIRE(load3->display_name == "Compressed Game 3");
}

TEST_CASE("Compression handles empty game data", "[save_manager][compression]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto empty_game = createEmptyTestGame();

    // Save with compression
    SaveSettings settings;
    settings.compress = true;
    settings.custom_name = "Empty Game Test";

    auto save_result = save_manager.saveGame(empty_game, settings);
    REQUIRE(save_result.has_value());

    std::string save_id = *save_result;

    // Load saved game
    auto load_result = save_manager.loadGame(save_id);
    REQUIRE(load_result.has_value());

    auto& loaded_game = *load_result;

    // Verify data matches (check individual fields for debugging)
    REQUIRE(loaded_game.current_state == empty_game.current_state);
    REQUIRE(loaded_game.original_puzzle == empty_game.original_puzzle);
    REQUIRE(loaded_game.difficulty == empty_game.difficulty);

    REQUIRE(loaded_game.notes.empty());

    // Note: Some fields like save_id, display_name, timestamps are set by SaveManager
    // so we don't check full equality via gamesAreEqual()

    // Verify all cells are zero
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            REQUIRE(loaded_game.current_state[row][col] == 0);
        }
    }
}

TEST_CASE("Decompression handles malformed data", "[save_manager][compression]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    // Create a file with valid zlib magic bytes but corrupted data
    auto malformed_file = temp_dir.path() / "malformed_save.yaml";

    {
        std::ofstream file(malformed_file, std::ios::binary);
        REQUIRE(file.is_open());

        // Write zlib magic bytes (0x78 0x9C for default compression)
        file.put(static_cast<char>(0x78));
        file.put(static_cast<char>(0x9C));

        // Write some random corrupted data
        const char corrupted[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        file.write(corrupted, sizeof(corrupted));
    }

    // Attempt to load the malformed file
    // Extract save_id from filename (remove .yaml extension)
    std::string save_id = malformed_file.stem().string();

    auto load_result = save_manager.loadGame(save_id);

    // Should return error, not crash
    REQUIRE_FALSE(load_result.has_value());

    // Verify it's a compression error or parse error
    // (Either is acceptable - compressed data is invalid)
    auto error = load_result.error();
    bool is_expected_error = (error == SaveError::CompressionError || error == SaveError::InvalidSaveData ||
                              error == SaveError::CorruptedData || error == SaveError::FileNotFound);
    REQUIRE(is_expected_error);
}

TEST_CASE("Compression handles large game state", "[save_manager][compression]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto game = createRichTestGame();

    // Add extensive notes to many cells (simulating heavy note usage)
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            if (game.current_state[row][col] == 0) {
                // Add several notes to each empty cell
                game.notes[row][col] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
            }
        }
    }

    // Simulate large history by increasing move count
    game.moves_made = 150;

    SECTION("Large game compresses well") {
        // Save with compression
        SaveSettings compressed_settings;
        compressed_settings.compress = true;
        compressed_settings.custom_name = "Large Game";

        auto compressed_save = save_manager.saveGame(game, compressed_settings);
        REQUIRE(compressed_save.has_value());

        // Save without compression for comparison
        SaveSettings uncompressed_settings;
        uncompressed_settings.compress = false;
        uncompressed_settings.custom_name = "Large Game Uncompressed";

        auto uncompressed_save = save_manager.saveGame(game, uncompressed_settings);
        REQUIRE(uncompressed_save.has_value());

        // Measure file sizes
        auto compressed_path = temp_dir.path() / (*compressed_save + ".yaml");
        auto uncompressed_path = temp_dir.path() / (*uncompressed_save + ".yaml");

        auto compressed_size = fs::file_size(compressed_path);
        auto uncompressed_size = fs::file_size(uncompressed_path);

        // Calculate reduction
        double reduction = 100.0 * (1.0 - static_cast<double>(compressed_size) / uncompressed_size);

        // Large files with repetitive data should compress very well (>50%)
        REQUIRE(reduction >= 50.0);
    }

    SECTION("Large game round-trip preserves all data") {
        // Save with compression
        SaveSettings settings;
        settings.compress = true;
        settings.custom_name = "Large Game Round-Trip";

        auto save_result = save_manager.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        // Load and verify
        auto load_result = save_manager.loadGame(*save_result);
        REQUIRE(load_result.has_value());

        auto& loaded = *load_result;

        // Verify all notes preserved
        REQUIRE(loaded.notes == game.notes);

        // Verify move count
        REQUIRE(loaded.moves_made == 150);

        // Verify all data
        REQUIRE(gamesAreEqual(game, loaded));
    }
}
