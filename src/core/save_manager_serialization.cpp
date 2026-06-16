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

#include "core/board_serializer.h"
#include "core/constants.h"
#include "core/file_utils.h"
#include "core/i_game_validator.h"
#include "core/i_puzzle_generator.h"
#include "core/i_save_manager.h"
#include "core/rating_version.h"
#include "encryption_manager.h"
#include "save_manager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace {
// Save file constants
// Bumped 1.0 → 1.1 when PuzzleOrigin was added (PR3 of user-provided-puzzle feature).
// Loaders still accept 1.0 saves; the missing `origin` key defaults to Generated for
// backward compatibility (regression-tested in test_save_origin_integration.cpp).
inline constexpr const char* SAVE_FILE_VERSION = "1.1";

// Schema versions the loader recognizes. A version outside this list still loads (best-effort,
// the schema only grows additively) but logs a warning. KEEP IN SYNC when bumping
// SAVE_FILE_VERSION — otherwise every existing save of the now-"old" version logs spuriously.
// constexpr std::array (not a std::unordered_set) so it has no throwing static initializer.
inline constexpr std::array<std::string_view, 2> kKnownSaveFormatVersions{"1.0", "1.1"};

// Difficulty validation constants (min/max values for range checking).
// MUST track the Difficulty enum in i_puzzle_generator.h; a hardcoded Expert
// bound silently rejected every Master save (see #11 / BL-8).
inline constexpr int MIN_DIFFICULTY = static_cast<int>(sudoku::core::Difficulty::Easy);
inline constexpr int MAX_DIFFICULTY = static_cast<int>(sudoku::core::Difficulty::Master);

// Zlib compression magic bytes
// All zlib compressed data starts with 0x78 followed by a compression level indicator
inline constexpr uint8_t ZLIB_MAGIC_BYTE = 0x78;
inline constexpr uint8_t ZLIB_NO_COMPRESSION = 0x01;       // No compression
inline constexpr uint8_t ZLIB_BEST_SPEED = 0x5E;           // Fast compression
inline constexpr uint8_t ZLIB_DEFAULT_COMPRESSION = 0x9C;  // Default compression
inline constexpr uint8_t ZLIB_BEST_COMPRESSION = 0xDA;     // Maximum compression
inline constexpr size_t ZLIB_HEADER_MIN_SIZE = 2;
}  // namespace

namespace sudoku::core {

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — serializes all game state sections into YAML; each section is necessary; cannot split without adding indirection
std::expected<void, SaveError> SaveManager::serializeToYaml(const SavedGame& game,
                                                            const std::filesystem::path& file_path,
                                                            const SaveSettings& settings) {
    try {
        YAML::Node root;

        // Metadata
        root["version"] = SAVE_FILE_VERSION;
        // Rating-model version (0b.0): persist the provenance of this game's stored rating
        // literals — the model version that produced them — so a future build can recognize
        // them as a snapshot and never silently reclassify them. Write the game's OWN value
        // (not the compile-time constant): a re-saved legacy game must keep its original, older
        // version so it stays recognizably stale. Producers stamp fresh ratings with the current
        // RATING_MODEL_VERSION. Always emitted (AC5c), independent of the schema version.
        root["rating_model_version"] = game.rating_model_version;
        root["save_id"] = game.save_id;
        root["display_name"] = game.display_name;
        root["created_time"] =
            std::chrono::duration_cast<std::chrono::seconds>(game.created_time.time_since_epoch()).count();
        root["last_modified"] =
            std::chrono::duration_cast<std::chrono::seconds>(game.last_modified.time_since_epoch()).count();

        // Puzzle data
        YAML::Node puzzle_data;
        puzzle_data["difficulty"] = static_cast<int>(game.difficulty);
        puzzle_data["puzzle_seed"] = game.puzzle_seed;
        puzzle_data["origin"] = static_cast<int>(game.origin);

        // Original puzzle
        puzzle_data["original_puzzle"] = BoardSerializer::serializeIntBoard(game.original_puzzle);

        // Current state
        puzzle_data["current_state"] = BoardSerializer::serializeIntBoard(game.current_state);

        // Notes (if not empty)
        if (!game.notes.empty()) {
            puzzle_data["notes"] = BoardSerializer::serializeNotes(game.notes);
        }

        // Serialize hint-revealed cells
        puzzle_data["hint_revealed_cells"] = BoardSerializer::serializeBoolBoard(game.hint_revealed_cells);

        root["puzzle_data"] = puzzle_data;

        // Game progress
        YAML::Node progress;
        progress["elapsed_time"] = game.elapsed_time.count();
        progress["moves_made"] = game.moves_made;
        progress["hints_used"] = game.hints_used;
        progress["mistakes"] = game.mistakes;
        progress["is_complete"] = game.is_complete;
        root["progress"] = progress;

        // Move history (if requested and not empty)
        if (settings.include_history && !game.move_history.empty()) {
            YAML::Node move_history;
            for (const auto& move : game.move_history) {
                YAML::Node yaml_move;
                yaml_move["row"] = move.position.row;
                yaml_move["col"] = move.position.col;
                yaml_move["value"] = move.value;
                yaml_move["type"] = static_cast<int>(move.move_type);
                yaml_move["is_note"] = move.is_note;
                // Move::timestamp was dropped in #24 M4 (write-only steady_clock value, meaningless
                // across restart, no consumer). The key is no longer written; the read path still
                // tolerates older snapshots that carry it (it is simply ignored).
                move_history.push_back(yaml_move);
            }
            root["move_history"] = move_history;
            root["current_move_index"] = game.current_move_index;
        }

        // Auto-save info
        if (game.is_auto_save) {
            root["is_auto_save"] = true;
            root["last_auto_save"] =
                std::chrono::duration_cast<std::chrono::seconds>(game.last_auto_save.time_since_epoch()).count();
        }

        // Puzzle rating
        if (game.puzzle_rating > 0) {
            root["puzzle_rating"] = game.puzzle_rating;
            root["puzzle_requires_backtracking"] = game.puzzle_requires_backtracking;
        }
        if (!game.puzzle_technique_ids.empty()) {
            YAML::Node technique_ids;
            for (int id : game.puzzle_technique_ids) {
                technique_ids.push_back(id);
            }
            root["puzzle_technique_ids"] = technique_ids;
        }

        // Write to file with optional compression
        // 1. Convert YAML to string
        std::stringstream yaml_stream;
        yaml_stream << root;
        std::string yaml_str = yaml_stream.str();

        // 2. Convert string to bytes
        std::vector<uint8_t> data(yaml_str.begin(), yaml_str.end());

        // 3. Optionally compress
        if (settings.compress) {
            auto compressed = compressData(data);
            if (!compressed) {
                return std::unexpected(compressed.error());
            }
            data = std::move(*compressed);
        }

        // 4. Optionally encrypt
        if (settings.encrypt) {
            auto encrypted = EncryptionManager::encrypt(data);
            if (!encrypted) {
                spdlog::error("Encryption failed");
                return std::unexpected(SaveError::EncryptionError);
            }
            data = std::move(*encrypted);
        }

        // 5. Write to file atomically (tmp + fsync + rename) so a crash or power
        //    loss mid-write can never truncate the save — auto-save is the
        //    recovery path, so an in-place write would defeat its purpose (#25).
        if (auto write_result = file_utils::atomicWriteFile(file_path, data); !write_result) {
            spdlog::error("Failed to write save file {}: {}", file_path.string(), write_result.error().message());
            return std::unexpected(SaveError::FileAccessError);
        }

        return {};

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML serialization error: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Serialization error: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — parses all game state sections from YAML; each section is necessary; cannot split without adding indirection
std::expected<SavedGame, SaveError> SaveManager::deserializeFromYaml(const std::filesystem::path& file_path) {
    try {
        // 1. Read file as binary
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(SaveError::FileNotFound);
        }
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // 2. Detect if encrypted (check for "SDKE" magic bytes)
        bool is_encrypted = EncryptionManager::isEncrypted(data);
        if (is_encrypted) {
            auto decrypted = EncryptionManager::decrypt(data);
            if (!decrypted) {
                spdlog::error("Decryption failed");
                return std::unexpected(SaveError::EncryptionError);
            }
            data = std::move(*decrypted);
        }

        // 3. Detect if compressed (zlib magic bytes)
        bool is_compressed = false;
        if (data.size() >= ZLIB_HEADER_MIN_SIZE && data[0] == ZLIB_MAGIC_BYTE) {
            // Check for valid zlib compression level indicators
            is_compressed = (data[1] == ZLIB_NO_COMPRESSION || data[1] == ZLIB_BEST_SPEED ||
                             data[1] == ZLIB_DEFAULT_COMPRESSION || data[1] == ZLIB_BEST_COMPRESSION);
        }

        // 4. Decompress if needed
        if (is_compressed) {
            auto decompressed = decompressData(data);
            if (!decompressed) {
                return std::unexpected(decompressed.error());
            }
            data = std::move(*decompressed);
        }

        // 5. Convert bytes to string and parse YAML
        std::string yaml_str(data.begin(), data.end());
        YAML::Node root = YAML::Load(yaml_str);

        SavedGame game;

        // Metadata
        // Schema version (0b.0: the loader reads `version` for the first time — it was a
        // write-only key before). Missing → "1.0" (the SavedGame default), mirroring the
        // origin-default back-compat pattern.
        if (root["version"]) {
            game.save_format_version = root["version"].as<std::string>();
        }
        // Unknown-version policy (AC5d): an unrecognized schema version is loaded best-effort
        // (the schema only ever grows additively) but logged — a defined branch, never UB and
        // never a hard reject. Known versions pass silently. ADD new schema versions to this set
        // when bumping SAVE_FILE_VERSION, or every existing save will log a spurious warning.
        if (std::ranges::find(kKnownSaveFormatVersions, game.save_format_version) == kKnownSaveFormatVersions.end()) {
            spdlog::warn("Unrecognized save schema version '{}' — attempting best-effort load",
                         game.save_format_version);
        }
        // Rating-model version (0b.0): which rating model produced the stored rating literals.
        // Missing → 0 (legacy / pre-hook, the SavedGame default). Read as a literal — it NEVER
        // drives a recompute; the snapshot-preserve decision happens after the rating is read.
        if (root["rating_model_version"]) {
            game.rating_model_version = root["rating_model_version"].as<int>();
        }
        if (root["save_id"]) {
            game.save_id = root["save_id"].as<std::string>();
        }
        if (root["display_name"]) {
            game.display_name = root["display_name"].as<std::string>();
        }
        if (root["created_time"]) {
            auto seconds = root["created_time"].as<long long>();
            game.created_time = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
        }
        if (root["last_modified"]) {
            auto seconds = root["last_modified"].as<long long>();
            game.last_modified = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
        }

        // Puzzle data
        if (!root["puzzle_data"]) {
            spdlog::error("Missing required field: puzzle_data");
            return std::unexpected(SaveError::SerializationError);
        }

        const auto& puzzle_data = root["puzzle_data"];

        // Validate required fields
        if (!puzzle_data["current_state"]) {
            spdlog::error("Missing required field: current_state");
            return std::unexpected(SaveError::SerializationError);
        }
        if (!puzzle_data["original_puzzle"]) {
            spdlog::error("Missing required field: original_puzzle");
            return std::unexpected(SaveError::SerializationError);
        }

        if (puzzle_data["difficulty"]) {
            int difficulty_val = puzzle_data["difficulty"].as<int>();
            if (difficulty_val < MIN_DIFFICULTY || difficulty_val > MAX_DIFFICULTY) {
                spdlog::error("Invalid difficulty value: {}", difficulty_val);
                return std::unexpected(SaveError::InvalidSaveData);
            }
            game.difficulty = static_cast<Difficulty>(difficulty_val);
        }
        if (puzzle_data["puzzle_seed"]) {
            game.puzzle_seed = puzzle_data["puzzle_seed"].as<uint32_t>();
        }

        // Origin (PR3): legacy saves omit this key — leave the SavedGame default (Generated).
        // Unknown enum values fall back to Generated rather than failing the load.
        if (puzzle_data["origin"]) {
            const int origin_val = puzzle_data["origin"].as<int>();
            switch (origin_val) {
                case static_cast<int>(PuzzleOrigin::Generated):
                case static_cast<int>(PuzzleOrigin::ImportedString):
                case static_cast<int>(PuzzleOrigin::ImportedEditMode):
                    game.origin = static_cast<PuzzleOrigin>(origin_val);
                    break;
                default:
                    spdlog::warn("Unknown PuzzleOrigin value in save: {} — defaulting to Generated", origin_val);
                    game.origin = PuzzleOrigin::Generated;
                    break;
            }
        }

        // Validate YAML board structure BEFORE deserialization
        if (puzzle_data["current_state"].size() != BOARD_SIZE) {
            spdlog::error("Invalid current_state dimensions in YAML: size={}", puzzle_data["current_state"].size());
            return std::unexpected(SaveError::InvalidSaveData);
        }
        if (puzzle_data["original_puzzle"].size() != BOARD_SIZE) {
            spdlog::error("Invalid original_puzzle dimensions in YAML: size={}", puzzle_data["original_puzzle"].size());
            return std::unexpected(SaveError::InvalidSaveData);
        }
        for (size_t i = 0; i < BOARD_SIZE; ++i) {
            if (puzzle_data["current_state"][i].size() != BOARD_SIZE) {
                spdlog::error("Invalid current_state row {} size in YAML: {}", i,
                              puzzle_data["current_state"][i].size());
                return std::unexpected(SaveError::InvalidSaveData);
            }
            if (puzzle_data["original_puzzle"][i].size() != BOARD_SIZE) {
                spdlog::error("Invalid original_puzzle row {} size in YAML: {}", i,
                              puzzle_data["original_puzzle"][i].size());
                return std::unexpected(SaveError::InvalidSaveData);
            }
        }

        // Original puzzle
        BoardSerializer::deserializeIntBoard(puzzle_data["original_puzzle"], game.original_puzzle);

        // Current state
        BoardSerializer::deserializeIntBoard(puzzle_data["current_state"], game.current_state);

        // Notes
        if (puzzle_data["notes"]) {
            BoardSerializer::deserializeNotes(puzzle_data["notes"], game.notes);
        }

        // Hint-revealed cells (with backward compatibility)
        if (puzzle_data["hint_revealed_cells"]) {
            BoardSerializer::deserializeBoolBoard(puzzle_data["hint_revealed_cells"], game.hint_revealed_cells);
        }
        // Backward compatibility: old saves without this field get default (all false)

        // Game progress
        if (root["progress"]) {
            const auto& progress = root["progress"];

            if (progress["elapsed_time"]) {
                game.elapsed_time = std::chrono::milliseconds(progress["elapsed_time"].as<long long>());
            }
            if (progress["moves_made"]) {
                game.moves_made = progress["moves_made"].as<int>();
            }
            if (progress["hints_used"]) {
                game.hints_used = progress["hints_used"].as<int>();
            }
            if (progress["mistakes"]) {
                game.mistakes = progress["mistakes"].as<int>();
            }
            if (progress["is_complete"]) {
                game.is_complete = progress["is_complete"].as<bool>();
            }
        }

        // Move history
        if (root["move_history"]) {
            const auto& move_history = root["move_history"];
            for (const auto& yaml_move : move_history) {
                Move move;
                move.position.row = yaml_move["row"].as<int>();
                move.position.col = yaml_move["col"].as<int>();
                move.value = yaml_move["value"].as<int>();
                move.move_type = static_cast<MoveType>(yaml_move["type"].as<int>());
                move.is_note = yaml_move["is_note"].as<bool>();

                // #24 M4: Move::timestamp removed. Older snapshots may still carry a "timestamp"
                // key — it is intentionally ignored (forward-tolerant read, no migration needed).

                game.move_history.push_back(move);
            }

            if (root["current_move_index"]) {
                game.current_move_index = root["current_move_index"].as<int>();
            }
        }

        // Auto-save info
        if (root["is_auto_save"]) {
            game.is_auto_save = root["is_auto_save"].as<bool>();
        }
        if (root["last_auto_save"]) {
            auto seconds = root["last_auto_save"].as<long long>();
            game.last_auto_save = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
        }

        // auto_notes_enabled: ignored (feature removed, backward-compatible)

        // Puzzle rating (backward-compatible: defaults to 0/empty if missing)
        if (root["puzzle_rating"]) {
            game.puzzle_rating = root["puzzle_rating"].as<double>();
        }
        if (root["puzzle_requires_backtracking"]) {
            game.puzzle_requires_backtracking = root["puzzle_requires_backtracking"].as<bool>();
        }
        if (root["puzzle_technique_ids"]) {
            for (const auto& id : root["puzzle_technique_ids"]) {
                game.puzzle_technique_ids.push_back(id.as<int>());
            }
        }

        // Snapshot-preserve policy (ADR, AC4): the rating literals above are kept exactly as
        // stored — never recomputed. SavedGame::isRatingStale() (computed from the now-read
        // rating_model_version) tells the UI whether they came from a different model so it can
        // offer an opt-in re-analysis. Auto-re-rate on load is intentionally NOT done: re-rating
        // needs per-step context the save does not carry, so it would require re-solving on the
        // load path (rejected).

        return game;

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML deserialization error: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    } catch (const std::exception& e) {
        spdlog::error("Deserialization error: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

}  // namespace sudoku::core
