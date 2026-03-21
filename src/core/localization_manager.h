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

#include "i_localization_manager.h"

#include <expected>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sudoku::core {

/// Production implementation of ILocalizationManager.
///
/// Loads localized strings from YAML files in a locales directory.
/// Falls back to English for missing keys, then to the key itself.
///
/// YAML file format:
///   locale: en
///   name: English
///   strings:
///     menu.game: "Game"
///     stats.games_played: "Games Played: {0}"
class LocalizationManager final : public ILocalizationManager {
public:
    /// Construct with path to locales directory.
    /// @param locales_dir Directory containing locale YAML files (e.g., en.yaml, de.yaml)
    explicit LocalizationManager(std::filesystem::path locales_dir);

    ~LocalizationManager() override = default;

    // Non-copyable, non-movable (singleton)
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;
    LocalizationManager(LocalizationManager&&) = delete;
    LocalizationManager& operator=(LocalizationManager&&) = delete;

    [[nodiscard]] std::string_view getString(std::string_view key) const override;

    [[nodiscard]] std::expected<void, std::string> setLocale(std::string_view locale_code) override;

    [[nodiscard]] std::string_view getCurrentLocale() const override;

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> getAvailableLocales() const override;

private:
    /// Load strings from a YAML file into the target map.
    /// @param file_path Path to the YAML locale file
    /// @param target Map to populate with key-value string pairs
    /// @return void on success, error message on failure
    [[nodiscard]] static std::expected<void, std::string>
    loadYamlFile(const std::filesystem::path& file_path, std::unordered_map<std::string, std::string>& target);

    /// Scan locales directory for available YAML files and populate available_locales_.
    void discoverLocales();

    std::filesystem::path locales_dir_;
    std::string current_locale_;
    std::unordered_map<std::string, std::string> strings_;           ///< Active locale strings
    std::unordered_map<std::string, std::string> fallback_strings_;  ///< English fallback strings

    /// Tracks missing keys to avoid repeated log warnings (mutable for const getString)
    mutable std::unordered_set<std::string> warned_missing_keys_;

    /// Cache for missing key strings (returns const char* to stored copies)
    mutable std::unordered_map<std::string, std::string> missing_key_cache_;

    /// Available locales discovered in the locales directory: (code, display_name)
    std::vector<std::pair<std::string, std::string>> available_locales_;
};

}  // namespace sudoku::core
