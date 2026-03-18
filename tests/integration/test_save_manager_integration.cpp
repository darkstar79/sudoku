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

#include <filesystem>
#include <fstream>
#include <tuple>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
namespace fs = std::filesystem;

// Test fixture for SaveManager integration tests
class SaveManagerTestFixture {
public:
    SaveManagerTestFixture()
        : test_dir_("./test_saves_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())),
          save_manager_(test_dir_) {
        // Create test directory
        fs::create_directories(test_dir_);
    }

    ~SaveManagerTestFixture() {
        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    SavedGame createTestGame(Difficulty diff = Difficulty::Medium) {
        SavedGame game;
        game.current_state = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                              {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                              {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};
        game.original_puzzle = game.current_state;
        game.notes = NotesData{};
        game.hint_revealed_cells = HintMaskData{};
        game.difficulty = diff;
        game.elapsed_time = std::chrono::milliseconds(12345);
        game.created_time = std::chrono::system_clock::now();
        return game;
    }

    std::string test_dir_;
    SaveManager save_manager_;
};

TEST_CASE("SaveManager - File I/O Integration", "[save_manager][integration]") {
    SaveManagerTestFixture fixture;

    SECTION("Save and load game round-trip") {
        auto original_game = fixture.createTestGame();

        // Save game
        SaveSettings settings;
        settings.compress = false;
        settings.encrypt = false;
        settings.custom_name = "Test Game";

        auto save_result = fixture.save_manager_.saveGame(original_game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;
        REQUIRE(!save_id.empty());

        // Load game
        auto load_result = fixture.save_manager_.loadGame(save_id);
        REQUIRE(load_result.has_value());

        auto& loaded_game = *load_result;

        // Verify data integrity
        REQUIRE(loaded_game.difficulty == original_game.difficulty);
        REQUIRE(loaded_game.current_state == original_game.current_state);
        REQUIRE(loaded_game.display_name == "Test Game");
        REQUIRE(loaded_game.save_id == save_id);
    }

    SECTION("Save multiple games") {
        auto game1 = fixture.createTestGame(Difficulty::Easy);
        auto game2 = fixture.createTestGame(Difficulty::Hard);
        auto game3 = fixture.createTestGame(Difficulty::Expert);

        SaveSettings settings;
        settings.custom_name = "Easy Game";
        auto save1 = fixture.save_manager_.saveGame(game1, settings);
        REQUIRE(save1.has_value());

        settings.custom_name = "Hard Game";
        auto save2 = fixture.save_manager_.saveGame(game2, settings);
        REQUIRE(save2.has_value());

        settings.custom_name = "Expert Game";
        auto save3 = fixture.save_manager_.saveGame(game3, settings);
        REQUIRE(save3.has_value());

        // List all saves
        auto list_result = fixture.save_manager_.listSaves();
        REQUIRE(list_result.has_value());
        REQUIRE(list_result->size() == 3);

        // Verify names
        std::vector<std::string> names;
        for (const auto& save : *list_result) {
            names.push_back(save.display_name);
        }
        REQUIRE(std::find(names.begin(), names.end(), "Easy Game") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "Hard Game") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "Expert Game") != names.end());
    }

    SECTION("Auto-save functionality") {
        auto game = fixture.createTestGame();

        // Test auto-save
        auto autosave_result = fixture.save_manager_.autoSave(game);
        REQUIRE(autosave_result.has_value());

        // Verify auto-save exists
        REQUIRE(fixture.save_manager_.hasAutoSave());

        // Load auto-save
        auto load_result = fixture.save_manager_.loadAutoSave();
        REQUIRE(load_result.has_value());

        auto& loaded = *load_result;
        REQUIRE(loaded.current_state == game.current_state);
        REQUIRE(loaded.difficulty == game.difficulty);
    }

    SECTION("Delete save file") {
        auto game = fixture.createTestGame();

        SaveSettings settings;
        settings.custom_name = "Game to Delete";
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;

        // Verify save exists
        auto load_before = fixture.save_manager_.loadGame(save_id);
        REQUIRE(load_before.has_value());

        // Delete save
        auto delete_result = fixture.save_manager_.deleteSave(save_id);
        REQUIRE(delete_result.has_value());

        // Verify save no longer exists
        auto load_after = fixture.save_manager_.loadGame(save_id);
        REQUIRE(!load_after.has_value());
        REQUIRE(load_after.error() == SaveError::FileNotFound);
    }

    SECTION("Rename save file") {
        auto game = fixture.createTestGame();

        SaveSettings settings;
        settings.custom_name = "Original Name";
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;

        // Rename save
        auto rename_result = fixture.save_manager_.renameSave(save_id, "New Name");
        REQUIRE(rename_result.has_value());

        // Load and verify new name
        auto load_result = fixture.save_manager_.loadGame(save_id);
        REQUIRE(load_result.has_value());
        REQUIRE(load_result->display_name == "New Name");
    }

    SECTION("Export and import save") {
        auto game = fixture.createTestGame();

        SaveSettings settings;
        settings.custom_name = "Export Test";
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;
        std::string export_path = fixture.test_dir_ + "/export_test.yaml";

        // Export save
        auto export_result = fixture.save_manager_.exportSave(save_id, export_path);
        REQUIRE(export_result.has_value());
        REQUIRE(fs::exists(export_path));

        // Import save
        auto import_result = fixture.save_manager_.importSave(export_path, "Imported Game");
        REQUIRE(import_result.has_value());

        std::string imported_id = *import_result;

        // Load imported save
        auto load_result = fixture.save_manager_.loadGame(imported_id);
        REQUIRE(load_result.has_value());
        REQUIRE(load_result->display_name == "Imported Game");
        REQUIRE(load_result->current_state == game.current_state);
    }

    SECTION("Get save info without loading") {
        auto game = fixture.createTestGame(Difficulty::Expert);

        SaveSettings settings;
        settings.custom_name = "Info Test";
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;

        // Get save info
        auto info_result = fixture.save_manager_.getSaveInfo(save_id);
        REQUIRE(info_result.has_value());

        REQUIRE(info_result->display_name == "Info Test");
        REQUIRE(info_result->difficulty == Difficulty::Expert);
        REQUIRE(info_result->save_id == save_id);
    }

    SECTION("Validate save file") {
        auto game = fixture.createTestGame();

        SaveSettings settings;
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;

        // Validate existing save
        REQUIRE(fixture.save_manager_.validateSave(save_id));

        // Validate non-existent save
        REQUIRE_FALSE(fixture.save_manager_.validateSave("nonexistent_id"));
    }

    SECTION("Error handling - load non-existent file") {
        auto load_result = fixture.save_manager_.loadGame("nonexistent_id");
        REQUIRE(!load_result.has_value());
        REQUIRE(load_result.error() == SaveError::FileNotFound);
    }

    SECTION("Error handling - export non-existent save") {
        auto export_result = fixture.save_manager_.exportSave("nonexistent_id", "/tmp/test.yaml");
        REQUIRE(!export_result.has_value());
        REQUIRE(export_result.error() == SaveError::FileNotFound);
    }

    SECTION("Error handling - import non-existent file") {
        auto import_result = fixture.save_manager_.importSave("/nonexistent/path.yaml", std::nullopt);
        REQUIRE(!import_result.has_value());
        REQUIRE(import_result.error() == SaveError::FileNotFound);
    }

    SECTION("Get save directory size") {
        // Create multiple saves
        for (int i = 0; i < 5; ++i) {
            auto game = fixture.createTestGame();
            SaveSettings settings;
            settings.custom_name = "Game " + std::to_string(i);
            std::ignore = fixture.save_manager_.saveGame(game, settings);
        }

        // Get directory size
        auto size = fixture.save_manager_.getSaveDirectorySize();
        REQUIRE(size > 0);
    }

    SECTION("Cleanup old saves") {
        // Create saves with different timestamps
        auto game = fixture.createTestGame();

        for (int i = 0; i < 3; ++i) {
            SaveSettings settings;
            settings.custom_name = "Old Game " + std::to_string(i);
            std::ignore = fixture.save_manager_.saveGame(game, settings);
        }

        // Attempt to cleanup saves older than 365 days (should not delete any)
        int deleted = fixture.save_manager_.cleanupOldSaves(365);
        REQUIRE(deleted == 0);

        // Verify all saves still exist
        auto list_result = fixture.save_manager_.listSaves();
        REQUIRE(list_result.has_value());
        REQUIRE(list_result->size() == 3);
    }
}

TEST_CASE("SaveManager - Move history persistence", "[save_manager][integration]") {
    SaveManagerTestFixture fixture;

    SECTION("Save and load move history") {
        auto game = fixture.createTestGame();

        // Add move history
        Move move1{.position = Position{.row = 0, .col = 2},
                   .value = 4,
                   .move_type = MoveType::PlaceNumber,
                   .is_note = false,
                   .timestamp = std::chrono::steady_clock::now(),
                   .previous_value = 0,
                   .previous_notes = {},
                   .previous_hint_revealed = false};
        Move move2{.position = Position{.row = 1, .col = 1},
                   .value = 7,
                   .move_type = MoveType::PlaceNumber,
                   .is_note = false,
                   .timestamp = std::chrono::steady_clock::now(),
                   .previous_value = 0,
                   .previous_notes = {},
                   .previous_hint_revealed = false};
        game.move_history = {move1, move2};

        // Save game
        SaveSettings settings;
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        // Load game
        auto load_result = fixture.save_manager_.loadGame(*save_result);
        REQUIRE(load_result.has_value());

        // Verify move history
        REQUIRE(load_result->move_history.size() == 2);
        REQUIRE(load_result->move_history[0].position.row == 0);
        REQUIRE(load_result->move_history[0].position.col == 2);
        REQUIRE(load_result->move_history[0].value == 4);
        REQUIRE(load_result->move_history[1].value == 7);
    }
}

TEST_CASE("SaveManager - Filesystem Error Paths", "[save_manager][integration][errors]") {
    SECTION("Read-only directory prevents saves") {
#ifdef _WIN32
        SKIP("Windows does not honour POSIX read-only directory permissions via std::filesystem");
#else
        SaveManagerTestFixture fixture;

        // Make directory read-only
        fs::permissions(fixture.test_dir_, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

        auto game = fixture.createTestGame();
        SaveSettings settings;

        auto result = fixture.save_manager_.saveGame(game, settings);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::FileAccessError);

        // Restore permissions for cleanup
        fs::permissions(fixture.test_dir_, fs::perms::owner_all, fs::perm_options::replace);
#endif
    }

    SECTION("Read-only directory prevents file deletion") {
#ifdef _WIN32
        SKIP("Windows does not honour POSIX read-only directory permissions via std::filesystem");
#else
        SaveManagerTestFixture fixture;

        // Create and save a game
        auto game = fixture.createTestGame();
        SaveSettings settings;
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());
        std::string save_id = *save_result;

        // Make the directory read-only (prevents file deletion on Linux)
        fs::permissions(fixture.test_dir_, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

        // Attempt to delete should fail (cannot modify read-only directory)
        auto delete_result = fixture.save_manager_.deleteSave(save_id);

        REQUIRE_FALSE(delete_result.has_value());
        REQUIRE(delete_result.error() == SaveError::FileAccessError);

        // Restore permissions for cleanup
        fs::permissions(fixture.test_dir_, fs::perms::owner_all, fs::perm_options::replace);
#endif
    }

    SECTION("Non-existent parent directory") {
        // Create SaveManager with non-existent directory path
        std::string non_existent = "./non_existent_dir_12345/saves";
        SaveManager manager(non_existent);

        auto game = SaveManagerTestFixture().createTestGame();
        SaveSettings settings;

        // Should create directory automatically (ensureDirectoryExists)
        auto result = manager.saveGame(game, settings);
        REQUIRE(result.has_value());  // Should succeed (creates directory)

        // Cleanup
        fs::remove_all("./non_existent_dir_12345");
    }

    SECTION("Corrupted YAML file fails to load") {
        SaveManagerTestFixture fixture;

        // Create a corrupted YAML file
        std::string save_id = "corrupted_yaml_test";
        auto save_path = fs::path(fixture.test_dir_) / (save_id + ".yaml");

        std::ofstream file(save_path);
        file << "this is not valid yaml!!!\n";
        file << "[missing colon\n";
        file << "invalid: {unclosed brace";
        file.close();

        auto result = fixture.save_manager_.loadGame(save_id);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::SerializationError);
    }

    SECTION("Missing required field (current_state)") {
        SaveManagerTestFixture fixture;

        std::string save_id = "missing_field_test";
        auto save_path = fs::path(fixture.test_dir_) / (save_id + ".yaml");

        // Create minimal YAML missing critical field
        std::ofstream file(save_path);
        file << "version: \"1.0\"\n";
        file << "save_id: \"" << save_id << "\"\n";
        file << "display_name: \"Test\"\n";
        file << "puzzle_data:\n";
        file << "  difficulty: 1\n";
        file << "  original_puzzle:\n";
        file << "    - [5,3,0,0,7,0,0,0,0]\n";
        file << "    - [6,0,0,1,9,5,0,0,0]\n";
        file << "    - [0,9,8,0,0,0,0,6,0]\n";
        file << "    - [8,0,0,0,6,0,0,0,3]\n";
        file << "    - [4,0,0,8,0,3,0,0,1]\n";
        file << "    - [7,0,0,0,2,0,0,0,6]\n";
        file << "    - [0,6,0,0,0,0,2,8,0]\n";
        file << "    - [0,0,0,4,1,9,0,0,5]\n";
        file << "    - [0,0,0,0,8,0,0,7,9]\n";
        // Missing current_state!
        file.close();

        auto result = fixture.save_manager_.loadGame(save_id);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::SerializationError);
    }

    SECTION("Invalid data types in YAML") {
        SaveManagerTestFixture fixture;

        std::string save_id = "invalid_types_test";
        auto save_path = fs::path(fixture.test_dir_) / (save_id + ".yaml");

        std::ofstream file(save_path);
        file << "version: \"1.0\"\n";
        file << "save_id: \"" << save_id << "\"\n";
        file << "puzzle_data:\n";
        file << "  difficulty: \"not_a_number\"\n";
        file << "  current_state: \"not_a_board\"\n";
        file.close();

        auto result = fixture.save_manager_.loadGame(save_id);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::SerializationError);
    }

    SECTION("All-zeros original_puzzle serializes successfully") {
        SaveManagerTestFixture fixture;

        // With fixed-size BoardData, "empty" boards are always valid 9x9 all-zeros.
        // The January 2026 bug (empty vector<vector<int>>) is structurally impossible now.
        SavedGame game;
        game.current_state = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                              {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                              {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};
        game.original_puzzle = {};  // All zeros — valid 9x9 board
        game.notes = NotesData{};
        game.hint_revealed_cells = HintMaskData{};
        game.difficulty = Difficulty::Easy;

        SaveSettings settings;

        auto result = fixture.save_manager_.saveGame(game, settings);

        // Should succeed — all-zeros is a valid (if useless) board
        REQUIRE(result.has_value());
    }

    SECTION("Corrupted zlib data fails decompression") {
        SaveManagerTestFixture fixture;

        // Create valid save first
        auto game = fixture.createTestGame();
        SaveSettings settings;
        settings.compress = true;
        auto save_result = fixture.save_manager_.saveGame(game, settings);
        REQUIRE(save_result.has_value());
        std::string save_id = *save_result;

        // Corrupt the compressed data
        auto save_path = fs::path(fixture.test_dir_) / (save_id + ".yaml");
        std::ifstream in(save_path, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        // Find and corrupt compressed data (flip bytes after magic)
        for (size_t i = 100; i < std::min(data.size(), size_t(200)); ++i) {
            data[i] ^= 0xFF;
        }

        std::ofstream out(save_path, std::ios::binary);
        out.write(reinterpret_cast<char*>(data.data()), data.size());
        out.close();

        auto result = fixture.save_manager_.loadGame(save_id);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::CompressionError);
    }

    SECTION("Out-of-range difficulty value") {
        SaveManagerTestFixture fixture;

        std::string save_id = "invalid_difficulty";
        auto save_path = fs::path(fixture.test_dir_) / (save_id + ".yaml");

        // Create YAML with invalid difficulty enum value
        std::ofstream file(save_path);
        file << "version: \"1.0\"\n";
        file << "save_id: \"" << save_id << "\"\n";
        file << "puzzle_data:\n";
        file << "  difficulty: 999\n";
        file << "  current_state:\n";
        file << "    - [5,3,0,0,7,0,0,0,0]\n";
        file << "    - [6,0,0,1,9,5,0,0,0]\n";
        file << "    - [0,9,8,0,0,0,0,6,0]\n";
        file << "    - [8,0,0,0,6,0,0,0,3]\n";
        file << "    - [4,0,0,8,0,3,0,0,1]\n";
        file << "    - [7,0,0,0,2,0,0,0,6]\n";
        file << "    - [0,6,0,0,0,0,2,8,0]\n";
        file << "    - [0,0,0,4,1,9,0,0,5]\n";
        file << "    - [0,0,0,0,8,0,0,7,9]\n";
        file << "  original_puzzle:\n";
        file << "    - [5,3,0,0,7,0,0,0,0]\n";
        file << "    - [6,0,0,1,9,5,0,0,0]\n";
        file << "    - [0,9,8,0,0,0,0,6,0]\n";
        file << "    - [8,0,0,0,6,0,0,0,3]\n";
        file << "    - [4,0,0,8,0,3,0,0,1]\n";
        file << "    - [7,0,0,0,2,0,0,0,6]\n";
        file << "    - [0,6,0,0,0,0,2,8,0]\n";
        file << "    - [0,0,0,4,1,9,0,0,5]\n";
        file << "    - [0,0,0,0,8,0,0,7,9]\n";
        file.close();

        auto result = fixture.save_manager_.loadGame(save_id);

        // Should fail with InvalidSaveData (999 is out of range 1-4)
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::InvalidSaveData);
    }

    SECTION("Board dimension mismatch (not 9x9)") {
        SaveManagerTestFixture fixture;

        std::string save_id = "wrong_dimensions";
        auto save_path = fs::path(fixture.test_dir_) / (save_id + ".yaml");

        std::ofstream file(save_path);
        file << "version: \"1.0\"\n";
        file << "save_id: \"" << save_id << "\"\n";
        file << "puzzle_data:\n";
        file << "  difficulty: 1\n";
        file << "  current_state:\n";
        file << "    - [1,2,3,4,5]\n";
        file << "    - [1,2,3,4,5]\n";
        file << "  original_puzzle:\n";
        file << "    - [1,2,3,4,5]\n";
        file << "    - [1,2,3,4,5]\n";
        file.close();

        auto result = fixture.save_manager_.loadGame(save_id);

        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SaveError::InvalidSaveData);
    }
}