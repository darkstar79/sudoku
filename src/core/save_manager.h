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

#include "i_save_manager.h"
#include "i_time_provider.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sudoku::core {

/// YAML-based implementation of ISaveManager
/// Handles game persistence with optional compression and encryption
class SaveManager : public ISaveManager {
public:
    /// @param save_directory Root directory for save files (defaults to the app save dir).
    /// @param time_provider Clock source for persisted wall-clock timestamps and the corrupt-save
    ///        archive suffix (#24). Defaults to SystemTimeProvider so existing callers are unchanged;
    ///        tests inject MockTimeProvider for deterministic timestamps.
    explicit SaveManager(std::filesystem::path save_directory = {},
                         std::shared_ptr<ITimeProvider> time_provider = std::make_shared<SystemTimeProvider>());
    ~SaveManager() override = default;
    SaveManager(const SaveManager&) = delete;
    SaveManager& operator=(const SaveManager&) = delete;
    SaveManager(SaveManager&&) = delete;
    SaveManager& operator=(SaveManager&&) = delete;

    // Core save/load operations
    [[nodiscard]] std::expected<std::string, SaveError> saveGame(const SavedGame& game,
                                                                 const SaveSettings& settings) override;

    [[nodiscard]] std::expected<SavedGame, SaveError> loadGame(const std::string& save_id) const override;

    // Auto-save operations
    [[nodiscard]] std::expected<void, SaveError> autoSave(const SavedGame& game) override;
    [[nodiscard]] std::expected<SavedGame, SaveError> loadAutoSave() override;
    [[nodiscard]] bool hasAutoSave() const override;
    [[nodiscard]] std::expected<void, SaveError> clearAutoSave() override;

    // Save management
    [[nodiscard]] std::expected<void, SaveError> deleteSave(const std::string& save_id) override;
    [[nodiscard]] std::expected<std::vector<SavedGame>, SaveError> listSaves() const override;
    [[nodiscard]] std::expected<SavedGame, SaveError> getSaveInfo(const std::string& save_id) const override;

    // Save utilities
    [[nodiscard]] std::expected<void, SaveError> renameSave(const std::string& save_id,
                                                            const std::string& new_name) override;

    // Import/Export
    [[nodiscard]] std::expected<void, SaveError> exportSave(const std::string& save_id,
                                                            const std::string& file_path) const override;

    [[nodiscard]] std::expected<std::string, SaveError> importSave(const std::string& file_path,
                                                                   const std::optional<std::string>& new_name) override;

    // Directory operations
    [[nodiscard]] std::string getSaveDirectory() const override;
    [[nodiscard]] bool validateSave(const std::string& save_id) const override;
    [[nodiscard]] int cleanupOldSaves(int days_old) override;
    [[nodiscard]] uint64_t getSaveDirectorySize() const override;

private:
    std::filesystem::path save_directory_;
    std::filesystem::path auto_save_path_;
    std::shared_ptr<ITimeProvider> time_provider_;

    // Helper methods
    [[nodiscard]] static std::string generateSaveId();
    [[nodiscard]] std::filesystem::path getSavePath(const std::string& save_id) const;
    [[nodiscard]] std::expected<void, SaveError> ensureDirectoryExists() const;

    /// Catastrophic-failure recovery (#24 / CODE_REVIEW §3.4): move an unreadable save file aside to
    /// `<path>.corrupt-<unix_ts>` (with a `-N` suffix on collision) instead of leaving it where a
    /// later save could overwrite it. The original bytes are preserved in the archive, never deleted.
    /// @return the archive path on success, or a SaveError if the rename failed.
    [[nodiscard]] std::expected<std::filesystem::path, SaveError>
    archiveCorruptSave(const std::filesystem::path& file_path) const;

    // YAML serialization
    [[nodiscard]] static std::expected<void, SaveError>
    serializeToYaml(const SavedGame& game, const std::filesystem::path& file_path, const SaveSettings& settings);

    [[nodiscard]] static std::expected<SavedGame, SaveError>
    deserializeFromYaml(const std::filesystem::path& file_path);

    // Compression helpers (future implementation)
    [[nodiscard]] static std::expected<std::vector<uint8_t>, SaveError> compressData(const std::vector<uint8_t>& data);

    [[nodiscard]] static std::expected<std::vector<uint8_t>, SaveError>
    decompressData(const std::vector<uint8_t>& data);

    // File validation
    [[nodiscard]] static bool isValidSaveFile(const std::filesystem::path& file_path);
    [[nodiscard]] static std::chrono::system_clock::time_point
    getFileModificationTime(const std::filesystem::path& file_path);
};

}  // namespace sudoku::core