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

#include "core/i_localization_manager.h"

#include <string>
#include <unordered_map>

namespace sudoku::core {

/// Lightweight mock for testing code that depends on ILocalizationManager.
/// Returns the key itself for getString(), allowing tests to verify
/// that localization lookups are performed without needing YAML files.
class MockLocalizationManager final : public ILocalizationManager {
public:
    [[nodiscard]] std::string_view getString(std::string_view key) const override {
        // Cache the key string so we can return a stable string_view
        auto [it, _] = key_cache_.try_emplace(std::string(key), std::string(key));
        return it->second;
    }

    [[nodiscard]] std::expected<void, std::string> setLocale(std::string_view /*locale_code*/) override {
        return {};
    }

    [[nodiscard]] std::string_view getCurrentLocale() const override {
        return "en";
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> getAvailableLocales() const override {
        return {{"en", "English"}};
    }

private:
    mutable std::unordered_map<std::string, std::string> key_cache_;
};

}  // namespace sudoku::core
