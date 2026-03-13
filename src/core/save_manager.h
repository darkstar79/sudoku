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

#include "i_save_manager.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sudoku::core {

// Forward declaration
class EncryptionManager;

/// YAML-based implementation of ISaveManager
/// Handles game persistence with optional compression and encryption
class SaveManager : public ISaveManager {
public:
    explicit SaveManager(std::filesystem::path save_directory = {});
    ~SaveManager() override;  // Destructor in .cpp due to unique_ptr<EncryptionManager>
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
    std::unique_ptr<EncryptionManager> encryption_manager_;

    // Helper methods
    [[nodiscard]] static std::string generateSaveId();
    [[nodiscard]] std::filesystem::path getSavePath(const std::string& save_id) const;
    [[nodiscard]] std::expected<void, SaveError> ensureDirectoryExists() const;

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