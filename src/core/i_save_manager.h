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

#pragma once

#include "core/board_data.h"
#include "core/rating_version.h"
#include "i_game_validator.h"
#include "i_puzzle_generator.h"

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sudoku::core {

/// Where a puzzle came from. Persisted to the save format so completed games can be
/// bucketed correctly in statistics (Generated puzzles vs. user-imported puzzles).
///
/// Values are pinned for save-file stability — do not renumber.
enum class PuzzleOrigin : std::uint8_t {
    Generated = 0,         ///< Created by PuzzleGenerator. Default for legacy / unmarked saves.
    ImportedString = 1,    ///< Imported via paste-string dialog.
    ImportedEditMode = 2,  ///< Imported via the board widget's Edit mode.
};

/// Represents a saved game state
struct SavedGame {
    // Game identification
    std::string save_id;
    std::string display_name;
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point last_modified;

    /// Save-format schema version, read from the file's `version` key (the loader read this
    /// key for the first time in story 0b.0 — it was write-only before). Missing key →
    /// "1.0" (pre-origin saves), mirroring the `origin`-default back-compat pattern. Tracked
    /// separately from rating_model_version: the schema and the rating model evolve on
    /// independent clocks.
    std::string save_format_version{"1.0"};

    // Puzzle data
    BoardData original_puzzle;         // Initial puzzle with clues
    BoardData current_state;           // Current board state
    NotesData notes;                   // 9x9 grid of note vectors
    HintMaskData hint_revealed_cells;  // 9x9 grid of hint-revealed flags
    Difficulty difficulty{};
    uint32_t puzzle_seed{};

    /// Where this puzzle came from — generator or user import. Default Generated so legacy
    /// saves (and any code path that builds a SavedGame without setting this) stay correct.
    PuzzleOrigin origin{PuzzleOrigin::Generated};

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

    /// Rating-model version that produced puzzle_rating / difficulty / puzzle_technique_ids.
    /// 0 = legacy / pre-hook save (key absent). This value is the *provenance* of the stored
    /// rating: it is written verbatim on save and read back on load, and NEVER triggers a
    /// recompute (see RATING_MODEL_VERSION in rating_version.h). Producers stamp it with the
    /// current model version when they freshly rate a puzzle; the load→save path preserves a
    /// loaded save's original version so a stale save stays recognizably stale across re-saves.
    int rating_model_version{0};

    /// True when this save's stored rating was produced by a different rating-model version than
    /// the running build — i.e. the stored rating/difficulty are a snapshot from an older (or
    /// newer) model and were NOT recomputed on load. Advisory only (e.g. a UI "re-analyze"
    /// affordance). Computed from rating_model_version rather than stored, so it is always
    /// correct regardless of how the SavedGame was constructed (load, ViewModel copy, factory).
    [[nodiscard]] bool isRatingStale() const {
        return rating_model_version != RATING_MODEL_VERSION;
    }

    // Move history for undo/redo
    std::vector<Move> move_history;
    int current_move_index{-1};  // For undo/redo functionality

    /// False when move_history is NOT a complete forward log back to original_puzzle — i.e. the
    /// board holds progress this save cannot explain. Auto-saves are written without history by
    /// design (SaveManager::autoSave sets include_history = false, story 6.5), so a game resumed
    /// from one, and every manual save later derived from it, has a truncated log through no fault
    /// of its own.
    ///
    /// This exists because the manual-save corruption guard (isCorruptedManualSave) treats "user
    /// values with an empty move history" as a phantom-value corruption tell. That inference is
    /// only sound when the writer intended to persist a full log. Story 8.16 / D2: without this
    /// flag, saving a resumed game produced a file the loader silently discarded, replacing the
    /// player's board with a freshly generated puzzle.
    ///
    /// Additive and defaulted TRUE, so pre-8.16 saves — which carry no such key — keep being judged
    /// by the heuristic exactly as before. Only a writer that knows its log is short says otherwise.
    /// No SAVE_FILE_VERSION bump: an absent key reads as the old behaviour (same back-compat pattern
    /// as `origin`), and the 1.0.0 format freeze holds.
    bool history_complete{true};

    // Auto-save info
    bool is_auto_save{false};
    std::chrono::system_clock::time_point last_auto_save;
};

/// Save operation settings
struct SaveSettings {
    bool compress{true};         // Compress save data
    bool include_history{true};  // Include move history
    /// Encrypt the save file. Live, not a future feature: manual saves set this, and the key is
    /// derived from machine identity — see EncryptionManager and SECURITY.md for what that means
    /// for portability. The default stays false so auto-save, export and any new caller opt in
    /// deliberately rather than inheriting encryption by accident.
    bool encrypt{false};
    std::string custom_name;  // Custom save name (empty for auto-generated)
};

/// The single definition of how a manual save is persisted.
///
/// Every path that writes a manual save — creating one, renaming one, importing one — must use
/// this. They used to build SaveSettings independently, so renaming or importing an encrypted save
/// silently rewrote it as plaintext (the default is `encrypt{false}`). Deliberately NOT used by:
/// auto-save, which is never encrypted, and exportSave, which writes plaintext and uncompressed on
/// purpose so an exported file is portable to another machine.
[[nodiscard]] inline SaveSettings manualSaveSettings(std::string custom_name = {}) {
    return SaveSettings{
        .compress = true, .include_history = true, .encrypt = true, .custom_name = std::move(custom_name)};
}

/// Error types for save/load operations
enum class SaveError : std::uint8_t {
    FileNotFound,
    FileAccessError,
    InvalidSaveData,
    CorruptedData,
    UnsupportedVersion,
    DiskFull,
    SaveIdExists,
    /// The supplied save id is not the 16-lowercase-hex form generateSaveId() produces. Save ids
    /// are read verbatim from file content, so they are untrusted input and are screened before
    /// any filesystem path is built from them.
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

    /// Removes the auto-save if one exists. Called when a game completes so a finished puzzle is not
    /// offered for resume on the next launch. Absent auto-save is treated as success (idempotent).
    /// @return Success, or an error if an existing auto-save could not be removed
    [[nodiscard]] virtual std::expected<void, SaveError> clearAutoSave() = 0;

    /// Deletes a saved game
    /// @param save_id Save ID to delete
    /// @return Success or error
    [[nodiscard]] virtual std::expected<void, SaveError> deleteSave(const std::string& save_id) = 0;

    /// Lists all available saved games.
    /// Each entry is a fully loaded SavedGame: everything the listing shows (name, difficulty,
    /// timestamps, rating) lives inside the possibly-compressed, possibly-encrypted blob, so there
    /// is no cheap metadata-only read. Cost is therefore proportional to the number of saves.
    /// @return Vector of saved games, newest first; entries that fail to parse are skipped
    [[nodiscard]] virtual std::expected<std::vector<SavedGame>, SaveError> listSaves() const = 0;

    /// Loads a single save by id.
    /// @note Despite the name this is NOT a cheap metadata read — it performs a full load,
    ///       identical in cost to loadGame(). No cheap metadata path exists (see listSaves).
    /// @param save_id Save ID to query; must be 16 lowercase hex chars or InvalidSaveId is returned
    /// @return The loaded save or error
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