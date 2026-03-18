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

/// Tests for SaveManager branch coverage:
/// - loadGame with non-existent save_id → FileNotFound
/// - loadAutoSave when no file exists → FileNotFound
/// - hasAutoSave true and false
/// - deleteSave with non-existent save_id → FileNotFound
/// - listSaves: auto-save file is skipped in listing
/// - listSaves: directory removed before call → empty list (line 220)
/// - renameSave with non-existent save_id → error
/// - exportSave with non-existent save_id → error
/// - importSave with non-existent file → FileNotFound
/// - importSave with corrupted YAML → SerializationError (line 306)
/// - importSave with/without new_name
/// - cleanupOldSaves when directory doesn't exist
/// - saveGame with empty display_name + custom_name
/// - saveGame with empty display_name and no custom_name (auto-generated)

#include "../../src/core/save_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
namespace fs = std::filesystem;

namespace {

class SaveBranchTempDir {
public:
    SaveBranchTempDir()
        : path_(fs::temp_directory_path() /
                ("sudoku_save_branch_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path_);
    }
    ~SaveBranchTempDir() {
        if (fs::exists(path_)) {
            fs::remove_all(path_);
        }
    }
    SaveBranchTempDir(const SaveBranchTempDir&) = delete;
    SaveBranchTempDir& operator=(const SaveBranchTempDir&) = delete;
    [[nodiscard]] const fs::path& path() const {
        return path_;
    }

private:
    fs::path path_;
};

SavedGame makeMinimalGame() {
    SavedGame game;
    game.current_state = BoardData{};
    game.original_puzzle = BoardData{};
    game.notes = NotesData{};
    game.hint_revealed_cells = HintMaskData{};
    game.difficulty = Difficulty::Easy;
    game.puzzle_seed = 1;
    game.display_name = "Test Game";
    return game;
}

}  // namespace

// ============================================================================
// loadGame: non-existent save_id
// ============================================================================

TEST_CASE("SaveManager - loadGame with non-existent save_id returns FileNotFound", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    auto result = mgr.loadGame("does-not-exist");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::FileNotFound);
}

// ============================================================================
// loadAutoSave: no auto-save file
// ============================================================================

TEST_CASE("SaveManager - loadAutoSave returns FileNotFound when no auto-save exists", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    auto result = mgr.loadAutoSave();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::FileNotFound);
}

// ============================================================================
// hasAutoSave: false and true
// ============================================================================

TEST_CASE("SaveManager - hasAutoSave returns false when no auto-save, true after autoSave", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SECTION("hasAutoSave is false before any auto-save") {
        REQUIRE_FALSE(mgr.hasAutoSave());
    }

    SECTION("hasAutoSave is true after autoSave") {
        SavedGame game = makeMinimalGame();
        auto result = mgr.autoSave(game);
        REQUIRE(result.has_value());
        REQUIRE(mgr.hasAutoSave());
    }
}

// ============================================================================
// deleteSave: non-existent save_id
// ============================================================================

TEST_CASE("SaveManager - deleteSave with non-existent save_id returns FileNotFound", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    auto result = mgr.deleteSave("nonexistent-id");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::FileNotFound);
}

// ============================================================================
// listSaves: auto-save is skipped; empty dir
// ============================================================================

TEST_CASE("SaveManager - listSaves skips auto-save file", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Create an auto-save
    SavedGame game = makeMinimalGame();
    REQUIRE(mgr.autoSave(game).has_value());

    // Create a regular save
    SaveSettings settings;
    settings.compress = false;
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    // listSaves should return only the regular save, not the auto-save
    auto list = mgr.listSaves();
    REQUIRE(list.has_value());
    REQUIRE(list->size() == 1);
}

TEST_CASE("SaveManager - listSaves returns empty when no saves exist", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    auto list = mgr.listSaves();
    REQUIRE(list.has_value());
    REQUIRE(list->empty());
}

// ============================================================================
// renameSave: non-existent save_id
// ============================================================================

TEST_CASE("SaveManager - renameSave with non-existent save_id returns error", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    auto result = mgr.renameSave("nonexistent-id", "New Name");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::FileNotFound);
}

TEST_CASE("SaveManager - renameSave with valid save_id updates display_name", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    SaveSettings settings;
    settings.compress = false;
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto rename_result = mgr.renameSave(*save_result, "Renamed Game");
    REQUIRE(rename_result.has_value());

    // Load and verify
    auto loaded = mgr.loadGame(*save_result);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->display_name == "Renamed Game");
}

// ============================================================================
// exportSave: non-existent save_id
// ============================================================================

TEST_CASE("SaveManager - exportSave with non-existent save_id returns error", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    auto result = mgr.exportSave("nonexistent-id", (tmp.path() / "export.yaml").string());
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::FileNotFound);
}

TEST_CASE("SaveManager - exportSave with valid save_id creates export file", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    SaveSettings settings;
    settings.compress = false;
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto export_path = (tmp.path() / "exported.yaml").string();
    auto export_result = mgr.exportSave(*save_result, export_path);
    REQUIRE(export_result.has_value());
    REQUIRE(fs::exists(export_path));
}

// ============================================================================
// importSave: non-existent file and with/without new_name
// ============================================================================

TEST_CASE("SaveManager - importSave with non-existent file returns FileNotFound", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    auto result = mgr.importSave("/nonexistent/path/save.yaml", std::nullopt);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SaveError::FileNotFound);
}

TEST_CASE("SaveManager - importSave without new_name appends (Imported)", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Create and export a save first
    SavedGame game = makeMinimalGame();
    game.display_name = "Original Name";
    SaveSettings settings;
    settings.compress = false;
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto export_path = (tmp.path() / "export_for_import.yaml").string();
    REQUIRE(mgr.exportSave(*save_result, export_path).has_value());

    // Import without new_name → appends "(Imported)"
    auto import_result = mgr.importSave(export_path, std::nullopt);
    REQUIRE(import_result.has_value());

    auto loaded = mgr.loadGame(*import_result);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->display_name == "Original Name (Imported)");
}

TEST_CASE("SaveManager - importSave with new_name uses provided name", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Create and export a save first
    SavedGame game = makeMinimalGame();
    game.display_name = "Original Name";
    SaveSettings settings;
    settings.compress = false;
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto export_path = (tmp.path() / "export_for_import2.yaml").string();
    REQUIRE(mgr.exportSave(*save_result, export_path).has_value());

    // Import with explicit new_name
    auto import_result = mgr.importSave(export_path, std::string("Custom Import Name"));
    REQUIRE(import_result.has_value());

    auto loaded = mgr.loadGame(*import_result);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->display_name == "Custom Import Name");
}

// ============================================================================
// saveGame with empty display_name
// ============================================================================

TEST_CASE("SaveManager - saveGame with empty display_name and custom_name uses custom_name",
          "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    game.display_name = "";  // Empty → triggers custom_name path

    SaveSettings settings;
    settings.compress = false;
    settings.custom_name = "My Custom Name";

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto loaded = mgr.loadGame(*save_result);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->display_name == "My Custom Name");
}

TEST_CASE("SaveManager - saveGame with empty display_name and no custom_name auto-generates name",
          "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    game.display_name = "";  // Empty → auto-generate from difficulty + timestamp

    SaveSettings settings;
    settings.compress = false;
    // No custom_name set → auto-generation path

    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    auto loaded = mgr.loadGame(*save_result);
    REQUIRE(loaded.has_value());
    // Auto-generated name starts with difficulty name
    REQUIRE_FALSE(loaded->display_name.empty());
    // Should start with "Easy" for Difficulty::Easy
    REQUIRE(loaded->display_name.rfind("Easy", 0) == 0);
}

// ============================================================================
// cleanupOldSaves: directory doesn't exist
// ============================================================================

TEST_CASE("SaveManager - cleanupOldSaves returns 0 when directory doesn't exist", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Remove the directory after construction
    fs::remove_all(tmp.path());

    int deleted = mgr.cleanupOldSaves(0);
    REQUIRE(deleted == 0);
}

// ============================================================================
// validateSave: valid and invalid save_id
// ============================================================================

TEST_CASE("SaveManager - validateSave returns false for non-existent save", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    REQUIRE_FALSE(mgr.validateSave("nonexistent-id"));
}

TEST_CASE("SaveManager - validateSave returns true for valid save", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    SaveSettings settings;
    settings.compress = false;
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    REQUIRE(mgr.validateSave(*save_result));
}

// ============================================================================
// autoSave + loadAutoSave roundtrip
// ============================================================================

TEST_CASE("SaveManager - autoSave then loadAutoSave roundtrip", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    game.current_state[0][0] = 5;

    auto save_result = mgr.autoSave(game);
    REQUIRE(save_result.has_value());

    auto load_result = mgr.loadAutoSave();
    REQUIRE(load_result.has_value());
    REQUIRE(load_result->is_auto_save == true);
    REQUIRE(load_result->current_state[0][0] == 5);
}

// ============================================================================
// deleteSave: existing save (success path)
// ============================================================================

TEST_CASE("SaveManager - deleteSave removes existing save", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    SaveSettings settings;
    settings.compress = false;
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    // Verify it exists
    REQUIRE(fs::exists(tmp.path() / (*save_result + ".yaml")));

    // Delete it
    auto del_result = mgr.deleteSave(*save_result);
    REQUIRE(del_result.has_value());

    // Verify it's gone
    REQUIRE_FALSE(fs::exists(tmp.path() / (*save_result + ".yaml")));
}

// ============================================================================
// Backward compat: load YAML without hint_revealed_cells
// ============================================================================

TEST_CASE("SaveManager - backward compat: load YAML without hint_revealed_cells", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Write a YAML file that is valid but lacks the hint_revealed_cells key
    // (simulates an older save format)
    auto save_path = tmp.path() / "compat-no-hints.yaml";
    {
        std::ofstream f(save_path);
        f << "version: '1.0'\n"
          << "save_id: compat-no-hints\n"
          << "display_name: Compat Test\n"
          << "created_time: 1000000000\n"
          << "last_modified: 1000000000\n"
          << "puzzle_data:\n"
          << "  difficulty: 0\n"
          << "  puzzle_seed: 42\n"
          << "  original_puzzle:\n";
        for (int r = 0; r < 9; ++r) {
            f << "    - [0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
        }
        f << "  current_state:\n";
        for (int r = 0; r < 9; ++r) {
            f << "    - [0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
        }
        // Intentionally omit hint_revealed_cells
        f << "progress:\n"
          << "  elapsed_time: 0\n"
          << "  moves_made: 0\n"
          << "  hints_used: 0\n"
          << "  mistakes: 0\n"
          << "  is_complete: false\n";
    }

    auto result = mgr.loadGame("compat-no-hints");
    REQUIRE(result.has_value());
    // Backward compat path should leave hint_revealed_cells as all-false (default)
    REQUIRE(result->hint_revealed_cells.empty());
}

// ============================================================================
// cleanupOldSaves: with actual saves present
// ============================================================================

TEST_CASE("SaveManager - cleanupOldSaves with negative days deletes all saves", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Create 2 saves
    SavedGame game = makeMinimalGame();
    SaveSettings settings;
    settings.compress = false;
    auto s1 = mgr.saveGame(game, settings);
    REQUIRE(s1.has_value());
    auto s2 = mgr.saveGame(game, settings);
    REQUIRE(s2.has_value());

    // Verify 2 saves exist
    auto list_before = mgr.listSaves();
    REQUIRE(list_before.has_value());
    REQUIRE(list_before->size() >= 2);

    // cleanup with -1 days: cutoff = now + 1 day → all recent files are older → delete all
    int deleted = mgr.cleanupOldSaves(-1);
    REQUIRE(deleted >= 2);

    auto list_after = mgr.listSaves();
    REQUIRE(list_after.has_value());
    REQUIRE(list_after->empty());
}

TEST_CASE("SaveManager - cleanupOldSaves with large days_old keeps all saves", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    SaveSettings settings;
    settings.compress = false;
    REQUIRE(mgr.saveGame(game, settings).has_value());

    // With 365 days, no recently-created file should be deleted
    int deleted = mgr.cleanupOldSaves(365);
    REQUIRE(deleted == 0);
}

// ============================================================================
// getSaveDirectorySize: empty and non-empty directory
// ============================================================================

TEST_CASE("SaveManager - getSaveDirectorySize returns 0 for empty directory", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    REQUIRE(mgr.getSaveDirectorySize() == 0);
}

TEST_CASE("SaveManager - getSaveDirectorySize returns positive after saving", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    SaveSettings settings;
    settings.compress = false;
    REQUIRE(mgr.saveGame(game, settings).has_value());

    REQUIRE(mgr.getSaveDirectorySize() > 0);
}

TEST_CASE("SaveManager - getSaveDirectory returns configured path", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // getSaveDirectory() is a simple getter that was not yet tested
    std::string dir = mgr.getSaveDirectory();
    REQUIRE(!dir.empty());
    REQUIRE(dir == tmp.path().string());
}

// ============================================================================
// saveGame with pre-set save_id (non-empty path in ternary at line 77)
// ============================================================================

TEST_CASE("SaveManager - saveGame with pre-set save_id reuses the same ID", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SavedGame game = makeMinimalGame();
    game.save_id = "my-custom-save-id";  // Non-empty → uses existing ID

    SaveSettings settings;
    settings.compress = false;
    auto result = mgr.saveGame(game, settings);
    REQUIRE(result.has_value());
    REQUIRE(*result == "my-custom-save-id");

    // Load back and verify
    auto loaded = mgr.loadGame("my-custom-save-id");
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->display_name == game.display_name);
}

// ============================================================================
// listSaves with multiple saves: exercises sort lambda (line 240)
// ============================================================================

TEST_CASE("SaveManager - listSaves returns saves sorted newest first", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Create 3 saves with different last_modified times (set explicitly)
    SaveSettings settings;
    settings.compress = false;

    using namespace std::chrono;
    auto now = system_clock::now();

    SavedGame game1 = makeMinimalGame();
    game1.display_name = "Oldest";
    game1.last_modified = now - hours(10);
    REQUIRE(mgr.saveGame(game1, settings).has_value());

    SavedGame game2 = makeMinimalGame();
    game2.display_name = "Newest";
    game2.last_modified = now;
    REQUIRE(mgr.saveGame(game2, settings).has_value());

    SavedGame game3 = makeMinimalGame();
    game3.display_name = "Middle";
    game3.last_modified = now - hours(5);
    REQUIRE(mgr.saveGame(game3, settings).has_value());

    // listSaves should return all 3 sorted by last_modified (newest first)
    auto list = mgr.listSaves();
    REQUIRE(list.has_value());
    REQUIRE(list->size() == 3);
    // Verify sort order: newest first
    REQUIRE(list->at(0).last_modified >= list->at(1).last_modified);
    REQUIRE(list->at(1).last_modified >= list->at(2).last_modified);
}

// ============================================================================
// cleanupOldSaves: autosave skipped and non-yaml file skipped (lines 353-355)
// ============================================================================

TEST_CASE("SaveManager - cleanupOldSaves skips autosave and non-yaml files", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    SaveSettings settings;
    settings.compress = false;

    // Create a regular save (will be deleted since days_old = -1 → future cutoff)
    SavedGame game = makeMinimalGame();
    auto save_result = mgr.saveGame(game, settings);
    REQUIRE(save_result.has_value());

    // Create an autosave (should be skipped, not deleted)
    REQUIRE(mgr.autoSave(game).has_value());

    // Create a non-yaml file in the save directory (should be skipped)
    auto txt_file = tmp.path() / "readme.txt";
    {
        std::ofstream f(txt_file);
        f << "not a save file\n";
    }
    REQUIRE(fs::exists(txt_file));

    // cleanupOldSaves with -1 days: future cutoff → all "old" saves deleted
    // But autosave should be skipped (line 354-355) and .txt should be skipped (line 353)
    int deleted = mgr.cleanupOldSaves(-1);
    REQUIRE(deleted == 1);  // Only the regular save is deleted

    // Autosave still exists
    REQUIRE(mgr.hasAutoSave());
    // Non-yaml file still exists
    REQUIRE(fs::exists(txt_file));
}

// ============================================================================
// listSaves returns empty when save directory has been removed (line 220)
// ============================================================================

TEST_CASE("SaveManager - listSaves returns empty when save directory removed", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Remove the directory entirely
    fs::remove_all(tmp.path());

    // listSaves: directory doesn't exist → returns empty list (covers line 220)
    auto list = mgr.listSaves();
    REQUIRE(list.has_value());
    REQUIRE(list->empty());
}

// ============================================================================
// importSave with file that exists but has corrupted YAML (line 306)
// ============================================================================

TEST_CASE("SaveManager - importSave with corrupted YAML returns error", "[save_manager_branches]") {
    SaveBranchTempDir tmp;
    SaveManager mgr(tmp.path().string());

    // Write a YAML file that exists but is unparseable
    auto bad_yaml = tmp.path() / "corrupted.yaml";
    {
        std::ofstream f(bad_yaml);
        f << "invalid: yaml: {{{broken\n";
    }

    // importSave: file exists → deserializeFromYaml fails → covers line 306
    auto result = mgr.importSave(bad_yaml.string(), std::nullopt);
    REQUIRE_FALSE(result.has_value());
}
