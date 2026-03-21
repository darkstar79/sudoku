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

#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sudoku::core {

/// Interface for localization services, enabling testable internationalization.
///
/// Production code uses LocalizationManager (loads YAML string files).
/// Test code uses MockLocalizationManager (returns keys as-is).
///
/// All getString() return values are std::string_view backed by stable storage.
class ILocalizationManager {
public:
    virtual ~ILocalizationManager() = default;

    /// Get a localized string by its key.
    /// @param key String key (e.g., "menu.game")
    /// @return Localized string, or the key itself if not found
    [[nodiscard]] virtual std::string_view getString(std::string_view key) const = 0;

    /// Switch the active locale by loading its YAML file.
    /// @param locale_code Locale code (e.g., "en", "de")
    /// @return void on success, error message on failure
    [[nodiscard]] virtual std::expected<void, std::string> setLocale(std::string_view locale_code) = 0;

    /// Get the currently active locale code.
    /// @return Locale code (e.g., "en")
    [[nodiscard]] virtual std::string_view getCurrentLocale() const = 0;

    /// Get all available locales discovered in the locales directory.
    /// @return Vector of (locale_code, display_name) pairs
    [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>> getAvailableLocales() const = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    ILocalizationManager() = default;
    ILocalizationManager(const ILocalizationManager&) = default;
    ILocalizationManager& operator=(const ILocalizationManager&) = default;
    ILocalizationManager(ILocalizationManager&&) = default;
    ILocalizationManager& operator=(ILocalizationManager&&) = default;
};

}  // namespace sudoku::core
