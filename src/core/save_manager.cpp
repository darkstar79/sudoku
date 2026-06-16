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

#include "save_manager.h"

#include "core/constants.h"
#include "core/i_save_manager.h"
#include "infrastructure/app_directory_manager.h"

#include <algorithm>
#include <array>
#include <compare>
#include <ctime>
#include <exception>
#include <optional>
#include <random>
#include <string_view>
#include <utility>

#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace {
// Save file constants
inline constexpr size_t SAVE_ID_LENGTH = 16;
inline constexpr int HEX_RADIX = 15;  // 0-15 for hexadecimal characters

// Time constants
inline constexpr int HOURS_PER_DAY = 24;
}  // namespace

namespace sudoku::core {

// NOLINTNEXTLINE(performance-unnecessary-value-param) — save_directory is std::move'd into save_directory_ below
SaveManager::SaveManager(std::filesystem::path save_directory, std::shared_ptr<ITimeProvider> time_provider)
    : time_provider_(std::move(time_provider)) {
    if (save_directory.empty()) {
        // Use platform-appropriate default directory
        save_directory_ =
            infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Saves);
    } else {
        save_directory_ = std::move(save_directory);
    }

    auto_save_path_ = save_directory_ / "autosave.yaml";

    auto result = ensureDirectoryExists();
    if (!result) {
        spdlog::warn("Failed to create save directory: {}", save_directory_.string());
    }
}

std::expected<std::string, SaveError> SaveManager::saveGame(const SavedGame& game, const SaveSettings& settings) {
    try {
        auto result = ensureDirectoryExists();
        if (!result) {
            return std::unexpected(SaveError::FileAccessError);
        }

        std::string save_id = game.save_id.empty() ? generateSaveId() : game.save_id;
        auto save_path = getSavePath(save_id);

        // Create a copy of the game with the save_id
        SavedGame game_copy = game;
        game_copy.save_id = save_id;
        game_copy.last_modified = time_provider_->systemNow();

        if (game_copy.display_name.empty()) {
            if (!settings.custom_name.empty()) {
                game_copy.display_name = settings.custom_name;
            } else {
                // Generate automatic name based on difficulty and timestamp
                constexpr std::array<std::string_view, DIFFICULTY_COUNT> difficulty_names = {"Easy", "Medium", "Hard",
                                                                                             "Expert", "Master"};
                static_assert(difficulty_names.size() == DIFFICULTY_COUNT,
                              "difficulty_names size must match DIFFICULTY_COUNT");
                auto now = time_provider_->systemNow();
                auto time_t_val = std::chrono::system_clock::to_time_t(now);
                std::tm tm_val{};
#ifdef _MSC_VER
                if (localtime_s(&tm_val, &time_t_val) == 0) {
#else
                if (localtime_r(&time_t_val, &tm_val) != nullptr) {
#endif
                    game_copy.display_name = fmt::format(
                        "{} Game {:%Y-%m-%d %H:%M}", difficulty_names[static_cast<int>(game_copy.difficulty)], tm_val);
                } else {
                    game_copy.display_name = difficulty_names[static_cast<int>(game_copy.difficulty)];
                }
            }
        }

        auto serialize_result = serializeToYaml(game_copy, save_path, settings);
        if (!serialize_result) {
            return std::unexpected(serialize_result.error());
        }

        spdlog::info("Game saved successfully: {} -> {}", game_copy.display_name, save_id);
        return save_id;

    } catch (const std::exception& e) {
        spdlog::error("Exception during save: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

std::expected<SavedGame, SaveError> SaveManager::loadGame(const std::string& save_id) const {
    try {
        auto save_path = getSavePath(save_id);

        if (!std::filesystem::exists(save_path)) {
            spdlog::warn("Save file not found: {}", save_path.string());
            return std::unexpected(SaveError::FileNotFound);
        }

        auto result = deserializeFromYaml(save_path);
        if (!result) {
            // Catastrophic-failure recovery (#24 / CODE_REVIEW §3.4): a corrupt/unparseable file is
            // archived aside so a later save can't overwrite recoverable bytes. Structurally-valid
            // but semantically-rejected saves (InvalidSaveData) are left in place.
            if (result.error() == SaveError::SerializationError) {
                (void)archiveCorruptSave(save_path);
            }
            return std::unexpected(result.error());
        }

        spdlog::info("Game loaded successfully: {}", save_id);
        return *result;

    } catch (const std::exception& e) {
        spdlog::error("Exception during load: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

std::expected<void, SaveError> SaveManager::autoSave(const SavedGame& game) {
    try {
        auto result = ensureDirectoryExists();
        if (!result) {
            return std::unexpected(SaveError::FileAccessError);
        }

        SavedGame auto_save_game = game;
        auto_save_game.is_auto_save = true;
        auto_save_game.last_auto_save = time_provider_->systemNow();
        auto_save_game.display_name = "Auto Save";

        SaveSettings settings;
        settings.compress = true;
        settings.include_history = false;  // Auto-saves don't need full history

        auto serialize_result = serializeToYaml(auto_save_game, auto_save_path_, settings);
        if (!serialize_result) {
            return std::unexpected(serialize_result.error());
        }

        spdlog::debug("Auto-save completed");
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during auto-save: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

std::expected<SavedGame, SaveError> SaveManager::loadAutoSave() {
    try {
        if (!std::filesystem::exists(auto_save_path_)) {
            return std::unexpected(SaveError::FileNotFound);
        }

        auto result = deserializeFromYaml(auto_save_path_);
        if (!result) {
            // Catastrophic-failure recovery (#24 / CODE_REVIEW §3.4): never overwrite an unreadable
            // auto-save — archive the corrupt bytes aside before the next autoSave() runs.
            if (result.error() == SaveError::SerializationError) {
                (void)archiveCorruptSave(auto_save_path_);
            }
            return std::unexpected(result.error());
        }

        spdlog::info("Auto-save loaded successfully");
        return *result;

    } catch (const std::exception& e) {
        spdlog::error("Exception during auto-save load: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

bool SaveManager::hasAutoSave() const {
    return std::filesystem::exists(auto_save_path_) && isValidSaveFile(auto_save_path_);
}

std::expected<void, SaveError> SaveManager::deleteSave(const std::string& save_id) {
    try {
        auto save_path = getSavePath(save_id);

        if (!std::filesystem::exists(save_path)) {
            return std::unexpected(SaveError::FileNotFound);
        }

        std::filesystem::remove(save_path);
        spdlog::info("Save deleted: {}", save_id);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during delete: {}", e.what());
        return std::unexpected(SaveError::FileAccessError);
    }
}

std::expected<std::vector<SavedGame>, SaveError> SaveManager::listSaves() const {
    try {
        std::vector<SavedGame> saves;

        if (!std::filesystem::exists(save_directory_)) {
            return saves;  // Return empty list if directory doesn't exist
        }

        for (const auto& entry : std::filesystem::directory_iterator(save_directory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
                if (entry.path() == auto_save_path_) {
                    continue;  // Skip auto-save file in listing
                }

                if (isValidSaveFile(entry.path())) {
                    auto load_result = deserializeFromYaml(entry.path());
                    if (load_result) {
                        saves.push_back(*load_result);
                    }
                }
            }
        }

        // Sort by last modified time (newest first)
        std::ranges::sort(
            saves, [](const SavedGame& lhs, const SavedGame& rhs) { return lhs.last_modified > rhs.last_modified; });

        return saves;

    } catch (const std::exception& e) {
        spdlog::error("Exception during list saves: {}", e.what());
        return std::unexpected(SaveError::FileAccessError);
    }
}

std::expected<SavedGame, SaveError> SaveManager::getSaveInfo(const std::string& save_id) const {
    return loadGame(save_id);
}

std::expected<void, SaveError> SaveManager::renameSave(const std::string& save_id, const std::string& new_name) {
    auto load_result = loadGame(save_id);
    if (!load_result) {
        return std::unexpected(load_result.error());
    }

    SavedGame game = *load_result;
    game.display_name = new_name;
    game.last_modified = time_provider_->systemNow();

    SaveSettings settings;
    auto save_result = saveGame(game, settings);
    if (!save_result) {
        return std::unexpected(save_result.error());
    }

    return {};
}

std::expected<void, SaveError> SaveManager::exportSave(const std::string& save_id, const std::string& file_path) const {
    auto load_result = loadGame(save_id);
    if (!load_result) {
        return std::unexpected(load_result.error());
    }

    try {
        SaveSettings settings;
        settings.compress = false;  // Don't compress exports for portability

        auto result = serializeToYaml(*load_result, file_path, settings);
        if (!result) {
            return std::unexpected(result.error());
        }

        spdlog::info("Save exported: {} -> {}", save_id, file_path);
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Exception during export: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

std::expected<std::string, SaveError> SaveManager::importSave(const std::string& file_path,
                                                              const std::optional<std::string>& new_name) {
    try {
        if (!std::filesystem::exists(file_path)) {
            return std::unexpected(SaveError::FileNotFound);
        }

        auto load_result = deserializeFromYaml(file_path);
        if (!load_result) {
            return std::unexpected(load_result.error());
        }

        SavedGame game = *load_result;
        game.save_id = generateSaveId();  // Generate new ID for import
        game.last_modified = time_provider_->systemNow();

        if (new_name) {
            game.display_name = *new_name;
        } else {
            game.display_name += " (Imported)";
        }

        SaveSettings settings;
        auto save_result = saveGame(game, settings);
        if (!save_result) {
            return std::unexpected(save_result.error());
        }

        spdlog::info("Save imported: {} -> {}", file_path, *save_result);
        return *save_result;

    } catch (const std::exception& e) {
        spdlog::error("Exception during import: {}", e.what());
        return std::unexpected(SaveError::SerializationError);
    }
}

std::string SaveManager::getSaveDirectory() const {
    return save_directory_.string();
}

bool SaveManager::validateSave(const std::string& save_id) const {
    auto save_path = getSavePath(save_id);
    return isValidSaveFile(save_path);
}

int SaveManager::cleanupOldSaves(int days_old) {
    try {
        int deleted_count = 0;
        auto cutoff_time = time_provider_->systemNow() - std::chrono::hours(HOURS_PER_DAY * days_old);

        if (!std::filesystem::exists(save_directory_)) {
            return 0;
        }

        for (const auto& entry : std::filesystem::directory_iterator(save_directory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
                if (entry.path() == auto_save_path_) {
                    continue;  // Don't delete auto-save
                }

                auto mod_time = getFileModificationTime(entry.path());
                if (mod_time < cutoff_time) {
                    std::filesystem::remove(entry.path());
                    deleted_count++;
                    spdlog::info("Cleaned up old save: {}", entry.path().filename().string());
                }
            }
        }

        return deleted_count;

    } catch (const std::exception& e) {
        spdlog::error("Exception during cleanup: {}", e.what());
        return 0;
    }
}

uint64_t SaveManager::getSaveDirectorySize() const {
    try {
        uint64_t total_size = 0;

        if (!std::filesystem::exists(save_directory_)) {
            return 0;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(save_directory_)) {
            if (entry.is_regular_file()) {
                total_size += entry.file_size();
            }
        }

        return total_size;

    } catch (const std::exception& e) {
        spdlog::error("Exception calculating directory size: {}", e.what());
        return 0;
    }
}

// Private helper methods

std::string SaveManager::generateSaveId() {
    static std::random_device rd;  // determinism-ok: save-id uniqueness entropy, not engine determinism
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, HEX_RADIX);

    const char* hex_chars = "0123456789abcdef";
    std::string save_id;
    save_id.reserve(SAVE_ID_LENGTH);

    for (size_t i = 0; i < SAVE_ID_LENGTH; ++i) {
        save_id += hex_chars[dis(gen)];
    }

    return save_id;
}

std::filesystem::path SaveManager::getSavePath(const std::string& save_id) const {
    return save_directory_ / (save_id + ".yaml");
}

std::expected<void, SaveError> SaveManager::ensureDirectoryExists() const {
    try {
        if (!std::filesystem::exists(save_directory_)) {
            std::filesystem::create_directories(save_directory_);
        }
        return {};
    } catch (const std::exception& e) {
        spdlog::error("Failed to create directory {}: {}", save_directory_.string(), e.what());
        return std::unexpected(SaveError::FileAccessError);
    }
}

std::expected<std::filesystem::path, SaveError>
SaveManager::archiveCorruptSave(const std::filesystem::path& file_path) const {
    // Mirrors StatisticsManager::archiveUnreadableSessions: stamp a `.corrupt-<unix_ts>` suffix and
    // uniquify with `-N` so a same-second retry never clobbers a prior archive (the "never deletes"
    // guarantee). systemNow() keeps the suffix deterministic under an injected clock in tests.
    auto timestamp = std::chrono::system_clock::to_time_t(time_provider_->systemNow());
    auto base_path = file_path;
    base_path += ".corrupt-" + std::to_string(timestamp);

    auto archive_path = base_path;
    for (int suffix = 1; std::filesystem::exists(archive_path); ++suffix) {
        archive_path = base_path;
        archive_path += "-" + std::to_string(suffix);
    }

    std::error_code ec;
    std::filesystem::rename(file_path, archive_path, ec);
    if (ec) {
        spdlog::error("Failed to archive corrupt save {} -> {}: {}", file_path.string(), archive_path.string(),
                      ec.message());
        return std::unexpected(SaveError::FileAccessError);
    }

    spdlog::warn("Archived unreadable save aside to {} (original not overwritten)", archive_path.string());
    return archive_path;
}

}  // namespace sudoku::core