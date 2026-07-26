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

#include "../../src/core/encryption_manager.h"
#include "../../src/core/save_manager.h"
#include "../helpers/test_utils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::test::TempTestDir;
namespace fs = std::filesystem;

// Helper: Create a SavedGame with test data
SavedGame createTestGame() {
    SavedGame game;

    game.current_state = sudoku::test::getEasyPuzzleWithPatterns();

    game.original_puzzle = game.current_state;

    game.notes = NotesData{};
    game.notes[0][2] = {1, 2, 4};
    game.notes[1][1] = {2, 4, 7};

    game.hint_revealed_cells = HintMaskData{};
    game.hint_revealed_cells.set(0, 3, true);

    game.difficulty = Difficulty::Medium;
    game.puzzle_seed = 42;
    game.elapsed_time = std::chrono::milliseconds(123456);
    game.moves_made = 15;
    game.hints_used = 2;
    game.mistakes = 1;
    game.is_complete = false;
    game.created_time = std::chrono::system_clock::now();
    game.last_modified = game.created_time;

    return game;
}

// Helper: Verify SavedGame equality
static bool gamesAreEqual(const SavedGame& a, const SavedGame& b) {
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

TEST_CASE("SaveManager encryption round-trip", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto original_game = createTestGame();

    SECTION("Encrypted save preserves all data") {
        SaveSettings settings;
        settings.compress = false;  // Test encryption only
        settings.encrypt = true;
        settings.custom_name = "Encrypted Test";

        auto save_result = save_manager.saveGame(original_game, settings);
        REQUIRE(save_result.has_value());

        std::string save_id = *save_result;

        auto load_result = save_manager.loadGame(save_id);
        REQUIRE(load_result.has_value());

        auto& loaded_game = *load_result;
        REQUIRE(gamesAreEqual(original_game, loaded_game));
    }

    SECTION("Encrypted + compressed save works") {
        SaveSettings settings;
        settings.compress = true;
        settings.encrypt = true;
        settings.custom_name = "Encrypted + Compressed";

        auto save_result = save_manager.saveGame(original_game, settings);
        REQUIRE(save_result.has_value());

        auto load_result = save_manager.loadGame(*save_result);
        REQUIRE(load_result.has_value());

        REQUIRE(gamesAreEqual(original_game, *load_result));
    }
}

TEST_CASE("SaveManager backward compatibility with unencrypted saves", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto original_game = createTestGame();

    SECTION("Can load unencrypted saves") {
        SaveSettings settings;
        settings.compress = false;
        settings.encrypt = false;
        settings.custom_name = "Unencrypted";

        auto save_result = save_manager.saveGame(original_game, settings);
        REQUIRE(save_result.has_value());

        auto load_result = save_manager.loadGame(*save_result);
        REQUIRE(load_result.has_value());
        REQUIRE(gamesAreEqual(original_game, *load_result));
    }

    SECTION("Can load compressed but unencrypted saves") {
        SaveSettings settings;
        settings.compress = true;
        settings.encrypt = false;
        settings.custom_name = "Compressed Only";

        auto save_result = save_manager.saveGame(original_game, settings);
        REQUIRE(save_result.has_value());

        auto load_result = save_manager.loadGame(*save_result);
        REQUIRE(load_result.has_value());
        REQUIRE(gamesAreEqual(original_game, *load_result));
    }
}

TEST_CASE("SaveManager mixed format handling", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto game1 = createTestGame();
    auto game2 = createTestGame();
    auto game3 = createTestGame();

    SECTION("List and load mixed saves") {
        // Save 1: Unencrypted, uncompressed
        SaveSettings settings1;
        settings1.compress = false;
        settings1.encrypt = false;
        settings1.custom_name = "Plain";
        auto save1 = save_manager.saveGame(game1, settings1);
        REQUIRE(save1.has_value());

        // Save 2: Compressed only
        SaveSettings settings2;
        settings2.compress = true;
        settings2.encrypt = false;
        settings2.custom_name = "Compressed";
        auto save2 = save_manager.saveGame(game2, settings2);
        REQUIRE(save2.has_value());

        // Save 3: Encrypted + compressed
        SaveSettings settings3;
        settings3.compress = true;
        settings3.encrypt = true;
        settings3.custom_name = "Both";
        auto save3 = save_manager.saveGame(game3, settings3);
        REQUIRE(save3.has_value());

        // List all saves
        auto list_result = save_manager.listSaves();
        REQUIRE(list_result.has_value());
        REQUIRE(list_result->size() == 3);

        // Load each save
        auto load1 = save_manager.loadGame(*save1);
        REQUIRE(load1.has_value());

        auto load2 = save_manager.loadGame(*save2);
        REQUIRE(load2.has_value());

        auto load3 = save_manager.loadGame(*save3);
        REQUIRE(load3.has_value());

        // Verify names
        REQUIRE(load1->display_name == "Plain");
        REQUIRE(load2->display_name == "Compressed");
        REQUIRE(load3->display_name == "Both");
    }
}

TEST_CASE("SaveManager encryption file format detection", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto game = createTestGame();

    SECTION("Encrypted file has correct magic bytes") {
        SaveSettings settings;
        settings.encrypt = true;
        settings.compress = false;
        settings.custom_name = "Magic Test";

        auto save_result = save_manager.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        // Read raw file
        auto file_path = temp_dir.path() / (*save_result + ".yaml");
        std::ifstream file(file_path, std::ios::binary);
        REQUIRE(file.is_open());

        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Check magic bytes "SDKE"
        REQUIRE(data.size() >= 4);
        REQUIRE(data[0] == 'S');
        REQUIRE(data[1] == 'D');
        REQUIRE(data[2] == 'K');
        REQUIRE(data[3] == 'E');
    }

    SECTION("Unencrypted file lacks magic bytes") {
        SaveSettings settings;
        settings.encrypt = false;
        settings.compress = false;
        settings.custom_name = "No Magic";

        auto save_result = save_manager.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        auto file_path = temp_dir.path() / (*save_result + ".yaml");
        std::ifstream file(file_path, std::ios::binary);
        REQUIRE(file.is_open());

        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Should NOT have "SDKE" magic bytes
        if (data.size() >= 4) {
            bool has_magic = (data[0] == 'S' && data[1] == 'D' && data[2] == 'K' && data[3] == 'E');
            REQUIRE_FALSE(has_magic);
        }
    }
}

TEST_CASE("SaveManager encryption error handling", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    SECTION("Corrupted encrypted file returns error") {
        auto game = createTestGame();

        // Create encrypted save
        SaveSettings settings;
        settings.encrypt = true;
        settings.custom_name = "Corrupt Test";

        auto save_result = save_manager.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        // Corrupt the file
        auto file_path = temp_dir.path() / (*save_result + ".yaml");
        std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);
        REQUIRE(file.is_open());

        // Seek to middle and corrupt data. Read the original byte first and
        // write its bitwise complement so the modification is guaranteed —
        // a fixed value would coincidentally match the original ~1/256 of
        // the time, leaving the file unchanged and the test flaky.
        file.seekg(100);
        char original_byte = 0;
        file.read(&original_byte, 1);
        file.seekp(100);
        char corrupt_byte = static_cast<char>(~original_byte);
        file.write(&corrupt_byte, 1);
        file.close();

        // Attempt to load
        auto load_result = save_manager.loadGame(*save_result);
        REQUIRE_FALSE(load_result.has_value());
        REQUIRE(load_result.error() == SaveError::EncryptionError);
    }
}

TEST_CASE("SaveManager validates encrypted saves", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto game = createTestGame();

    SECTION("Valid encrypted save validates correctly") {
        SaveSettings settings;
        settings.encrypt = true;
        settings.custom_name = "Valid Encrypted";

        auto save_result = save_manager.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        REQUIRE(save_manager.validateSave(*save_result));
    }

    SECTION("Corrupted encrypted save fails validation") {
        SaveSettings settings;
        settings.encrypt = true;
        settings.custom_name = "Invalid Encrypted";

        auto save_result = save_manager.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        // Corrupt file
        auto file_path = temp_dir.path() / (*save_result + ".yaml");
        std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
        file << "corrupted data";
        file.close();

        REQUIRE_FALSE(save_manager.validateSave(*save_result));
    }
}

TEST_CASE("SaveManager auto-save with encryption", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto game = createTestGame();

    SECTION("Auto-save uses encryption when enabled") {
        // Note: auto-save settings are typically configured globally
        // For this test, we verify the mechanism works
        auto result = save_manager.autoSave(game);
        REQUIRE(result.has_value());

        REQUIRE(save_manager.hasAutoSave());

        auto loaded = save_manager.loadAutoSave();
        REQUIRE(loaded.has_value());
        REQUIRE(gamesAreEqual(game, *loaded));
    }
}

TEST_CASE("SaveManager export with encryption", "[save_manager][encryption]") {
    TempTestDir temp_dir;
    SaveManager save_manager(temp_dir.path().string());

    auto game = createTestGame();

    SECTION("Export encrypted save to external file") {
        SaveSettings settings;
        settings.encrypt = true;
        settings.compress = true;
        settings.custom_name = "Export Test";

        auto save_result = save_manager.saveGame(game, settings);
        REQUIRE(save_result.has_value());

        auto export_path = temp_dir.path() / "exported_game.yaml";
        auto export_result = save_manager.exportSave(*save_result, export_path.string());
        REQUIRE(export_result.has_value());

        REQUIRE(fs::exists(export_path));

        // Import back
        auto import_result = save_manager.importSave(export_path.string(), "Imported");
        REQUIRE(import_result.has_value());

        auto loaded = save_manager.loadGame(*import_result);
        REQUIRE(loaded.has_value());
        REQUIRE(gamesAreEqual(game, *loaded));
    }
}

// ============================================================================
// Persistence policy (story 8-4 / SAVE-6): every write path that produces a manual save must
// agree on the encryption state, and the KDF tier is INTERACTIVE.
// ============================================================================

namespace {

// Reads the encryption envelope's header bytes: "SDKE" magic, version, flags.
struct SaveEnvelope {
    bool encrypted{false};
    bool interactive_kdf{false};
};

SaveEnvelope readEnvelope(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    std::array<char, 6> header{};
    in.read(header.data(), header.size());
    REQUIRE(in.gcount() == static_cast<std::streamsize>(header.size()));

    SaveEnvelope envelope;
    envelope.encrypted = header[0] == 'S' && header[1] == 'D' && header[2] == 'K' && header[3] == 'E';
    envelope.interactive_kdf = envelope.encrypted && (static_cast<unsigned char>(header[5]) & 0x01U) != 0;
    return envelope;
}

}  // namespace

TEST_CASE("SaveManager writes manual saves with the INTERACTIVE KDF tier", "[save_manager][encryption][policy]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SaveSettings settings;
    settings.encrypt = true;
    settings.compress = true;
    auto id = mgr.saveGame(createTestGame(), settings);
    REQUIRE(id.has_value());

    auto envelope = readEnvelope(tmp.path() / (*id + ".yaml"));

    REQUIRE(envelope.encrypted);
    REQUIRE(envelope.interactive_kdf);
}

TEST_CASE("SaveManager rename preserves the encrypted state of a save", "[save_manager][encryption][policy]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    SaveSettings settings;
    settings.encrypt = true;
    settings.compress = true;
    auto id = mgr.saveGame(createTestGame(), settings);
    REQUIRE(id.has_value());
    REQUIRE(readEnvelope(tmp.path() / (*id + ".yaml")).encrypted);

    // Before 8-4 this re-saved with default SaveSettings and silently rewrote the file as
    // plaintext — an encrypted save quietly losing its encryption on a rename.
    auto renamed = mgr.renameSave(*id, "Renamed");
    REQUIRE(renamed.has_value());

    auto envelope = readEnvelope(tmp.path() / (*id + ".yaml"));

    REQUIRE(envelope.encrypted);
    REQUIRE(envelope.interactive_kdf);

    auto loaded = mgr.loadGame(*id);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->display_name == "Renamed");
}

TEST_CASE("SaveManager import stores the imported save under the manual-save policy",
          "[save_manager][encryption][policy]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Export writes a portable plaintext file by design; importing it must still land an encrypted
    // save in the save directory, matching every other manual save.
    SaveSettings settings;
    settings.encrypt = true;
    auto id = mgr.saveGame(createTestGame(), settings);
    REQUIRE(id.has_value());

    const auto exported = tmp.path() / "exported_save.yaml";
    REQUIRE(mgr.exportSave(*id, exported.string()).has_value());
    REQUIRE_FALSE(readEnvelope(exported).encrypted);  // portability: export stays plaintext

    auto imported_id = mgr.importSave(exported.string(), "Imported");
    REQUIRE(imported_id.has_value());

    auto envelope = readEnvelope(tmp.path() / (*imported_id + ".yaml"));

    REQUIRE(envelope.encrypted);
    REQUIRE(envelope.interactive_kdf);
}

TEST_CASE("SaveManager loads saves written with either KDF tier", "[save_manager][encryption][policy]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Back-compat: the tier lives in the flags byte and decrypt() dispatches on it, so a save
    // written with the older MODERATE cost keeps loading alongside new INTERACTIVE ones.
    SaveSettings settings;
    settings.encrypt = true;
    settings.compress = false;
    auto id = mgr.saveGame(createTestGame(), settings);
    REQUIRE(id.has_value());

    const auto interactive_path = tmp.path() / (*id + ".yaml");
    std::vector<uint8_t> plain;
    {
        std::ifstream in(interactive_path, std::ios::binary);
        const std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto decrypted = EncryptionManager::decrypt(blob);
        REQUIRE(decrypted.has_value());
        plain = *decrypted;
    }

    // Re-encrypt the very same payload at MODERATE cost and drop it in as a second save.
    auto moderate_blob = EncryptionManager::encrypt(plain);
    REQUIRE(moderate_blob.has_value());
    const std::string moderate_id = sudoku::test::saveIdFor("moderate_legacy");
    {
        std::ofstream out(tmp.path() / (moderate_id + ".yaml"), std::ios::binary);
        out.write(reinterpret_cast<const char*>(moderate_blob->data()),
                  static_cast<std::streamsize>(moderate_blob->size()));
    }

    auto interactive_loaded = mgr.loadGame(*id);
    auto moderate_loaded = mgr.loadGame(moderate_id);

    REQUIRE(interactive_loaded.has_value());
    REQUIRE(moderate_loaded.has_value());
    REQUIRE(gamesAreEqual(*interactive_loaded, *moderate_loaded));
}

TEST_CASE("SaveManager rename brings a plaintext save under the manual-save policy",
          "[save_manager][encryption][policy]") {
    TempTestDir tmp;
    SaveManager mgr(tmp.path().string());

    // Deliberate: the policy is applied, not merely preserved, so the save directory converges on
    // one encryption state. The change is an upgrade (plaintext -> encrypted) and never the
    // reverse, which is the silent downgrade this story exists to remove.
    SaveSettings plaintext;
    plaintext.encrypt = false;
    plaintext.compress = false;
    auto id = mgr.saveGame(createTestGame(), plaintext);
    REQUIRE(id.has_value());
    REQUIRE_FALSE(readEnvelope(tmp.path() / (*id + ".yaml")).encrypted);

    REQUIRE(mgr.renameSave(*id, "Now Encrypted").has_value());

    auto envelope = readEnvelope(tmp.path() / (*id + ".yaml"));

    REQUIRE(envelope.encrypted);
    REQUIRE(envelope.interactive_kdf);
}
