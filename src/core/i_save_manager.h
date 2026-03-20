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

#pragma once

#include "core/board_data.h"
#include "i_game_validator.h"
#include "i_puzzle_generator.h"

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace sudoku::core {

/// Represents a saved game state
struct SavedGame {
    // Game identification
    std::string save_id;
    std::string display_name;
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point last_modified;

    // Puzzle data
    BoardData original_puzzle;         // Initial puzzle with clues
    BoardData current_state;           // Current board state
    NotesData notes;                   // 9x9 grid of note vectors
    HintMaskData hint_revealed_cells;  // 9x9 grid of hint-revealed flags
    Difficulty difficulty{};
    uint32_t puzzle_seed{};

    // Game progress
    std::chrono::milliseconds elapsed_time{0};
    int moves_made{0};
    int hints_used{0};
    int mistakes{0};
    bool is_complete{false};

    // Puzzle rating info
    double puzzle_rating{0.0};
    std::vector<int> puzzle_technique_ids;  // SolvingTechnique enum values (locale-independent)
    bool puzzle_requires_backtracking{false};

    // Move history for undo/redo
    std::vector<Move> move_history;
    int current_move_index{-1};  // For undo/redo functionality

    // Auto-save info
    bool is_auto_save{false};
    std::chrono::system_clock::time_point last_auto_save;
};

/// Save operation settings
struct SaveSettings {
    bool compress{true};         // Compress save data
    bool include_history{true};  // Include move history
    bool encrypt{false};         // Encrypt save file (future feature)
    std::string custom_name;     // Custom save name (empty for auto-generated)
};

/// Error types for save/load operations
enum class SaveError : std::uint8_t {
    FileNotFound,
    FileAccessError,
    InvalidSaveData,
    CorruptedData,
    UnsupportedVersion,
    DiskFull,
    SaveIdExists,
    InvalidSaveId,
    SerializationError,
    CompressionError,
    EncryptionError
};

/// Interface for managing game saves and auto-save functionality
class ISaveManager {
public:
    virtual ~ISaveManager() = default;

    /// Saves the current game state
    /// @param game Current game state to save
    /// @param settings Save operation settings
    /// @return Save ID or error
    virtual std::expected<std::string, SaveError> saveGame(const SavedGame& game,
                                                           const SaveSettings& settings = {}) = 0;

    /// Loads a saved game by ID
    /// @param save_id Unique identifier for the saved game
    /// @return Loaded game state or error
    [[nodiscard]] virtual std::expected<SavedGame, SaveError> loadGame(const std::string& save_id) const = 0;

    /// Auto-saves the current game (single slot)
    /// @param game Current game state
    /// @return Success or error
    virtual std::expected<void, SaveError> autoSave(const SavedGame& game) = 0;

    /// Loads the auto-saved game if it exists
    /// @return Auto-saved game or error if none exists
    virtual std::expected<SavedGame, SaveError> loadAutoSave() = 0;

    /// Checks if an auto-saved game exists
    /// @return True if auto-save exists
    [[nodiscard]] virtual bool hasAutoSave() const = 0;

    /// Deletes a saved game
    /// @param save_id Save ID to delete
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, SaveError> deleteSave(const std::string& save_id) = 0;

    /// Lists all available saved games
    /// @return Vector of save metadata (without full game data)
    [[nodiscard]] virtual std::expected<std::vector<SavedGame>, SaveError> listSaves() const = 0;

    /// Gets quick info about a save without loading full data
    /// @param save_id Save ID to query
    /// @return Save metadata or error
    [[nodiscard]] virtual std::expected<SavedGame, SaveError> getSaveInfo(const std::string& save_id) const = 0;

    /// Renames a saved game
    /// @param save_id Save ID to rename
    /// @param new_name New display name
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, SaveError> renameSave(const std::string& save_id,
                                                                    const std::string& new_name) = 0;

    /// Exports a save to a specific file path
    /// @param save_id Save ID to export
    /// @param file_path Target file path
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, SaveError> exportSave(const std::string& save_id,
                                                                    const std::string& file_path) const = 0;

    /// Imports a save from a file
    /// @param file_path Source file path
    /// @param new_name Optional custom name for imported save
    /// @return New save ID or error
    [[nodiscard]] virtual std::expected<std::string, SaveError>
    importSave(const std::string& file_path, const std::optional<std::string>& new_name = std::nullopt) = 0;

    /// Gets the default save directory path
    /// @return Platform-specific save directory
    [[nodiscard]] virtual std::string getSaveDirectory() const = 0;

    /// Validates save file integrity
    /// @param save_id Save ID to validate
    /// @return True if valid, false if corrupted
    [[nodiscard]] virtual bool validateSave(const std::string& save_id) const = 0;

    /// Cleans up old auto-saves and temporary files
    /// @param days_old Delete auto-saves older than this many days
    /// @return Number of files cleaned up
    virtual int cleanupOldSaves(int days_old = 30) = 0;

    /// Gets disk space used by all saves
    /// @return Size in bytes
    [[nodiscard]] virtual uint64_t getSaveDirectorySize() const = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    ISaveManager() = default;
    ISaveManager(const ISaveManager&) = default;
    ISaveManager& operator=(const ISaveManager&) = default;
    ISaveManager(ISaveManager&&) = default;
    ISaveManager& operator=(ISaveManager&&) = default;
};

}  // namespace sudoku::core