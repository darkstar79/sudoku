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

#include "localization_manager.h"

#include <algorithm>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::core {

LocalizationManager::LocalizationManager(std::filesystem::path locales_dir) : locales_dir_(std::move(locales_dir)) {
    discoverLocales();
}

std::string_view LocalizationManager::getString(std::string_view key) const {
    // Look up in active locale first
    auto it = strings_.find(std::string(key));
    if (it != strings_.end()) {
        return it->second;
    }

    // Fall back to English
    auto fallback_it = fallback_strings_.find(std::string(key));
    if (fallback_it != fallback_strings_.end()) {
        return fallback_it->second;
    }

    // Key not found in any locale — warn once and return the key itself
    std::string key_str(key);
    if (!warned_missing_keys_.contains(key_str)) {
        spdlog::warn("Localization: missing key '{}' in locale '{}' and fallback", key_str, current_locale_);
        warned_missing_keys_.insert(key_str);
    }

    // Cache the key string so we can return a stable string_view
    auto [cache_it, inserted] = missing_key_cache_.try_emplace(key_str, key_str);
    return cache_it->second;
}

std::expected<void, std::string> LocalizationManager::setLocale(std::string_view locale_code) {
    auto locale_file = locales_dir_ / (std::string(locale_code) + ".yaml");

    if (!std::filesystem::exists(locale_file)) {
        return std::unexpected("Locale file not found: " + locale_file.string());
    }

    // Load fallback (English) if not already loaded and we're switching to non-English
    if (fallback_strings_.empty() && locale_code != "en") {
        auto en_file = locales_dir_ / "en.yaml";
        if (std::filesystem::exists(en_file)) {
            auto result = loadYamlFile(en_file, fallback_strings_);
            if (!result) {
                spdlog::warn("Failed to load English fallback: {}", result.error());
            }
        }
    }

    // Load the requested locale
    std::unordered_map<std::string, std::string> new_strings;
    auto result = loadYamlFile(locale_file, new_strings);
    if (!result) {
        return result;
    }

    strings_ = std::move(new_strings);
    current_locale_ = std::string(locale_code);

    // If we just loaded English, also use it as fallback
    if (locale_code == "en") {
        fallback_strings_ = strings_;
    }

    // Clear warning caches since locale changed
    warned_missing_keys_.clear();
    missing_key_cache_.clear();

    spdlog::info("Locale set to '{}' ({} strings loaded)", current_locale_, strings_.size());
    return {};
}

std::string_view LocalizationManager::getCurrentLocale() const {
    return current_locale_;
}

std::vector<std::pair<std::string, std::string>> LocalizationManager::getAvailableLocales() const {
    return available_locales_;
}

std::expected<void, std::string>
LocalizationManager::loadYamlFile(const std::filesystem::path& file_path,
                                  std::unordered_map<std::string, std::string>& target) {
    try {
        YAML::Node root = YAML::LoadFile(file_path.string());

        if (!root["strings"]) {
            return std::unexpected("YAML file missing 'strings' section: " + file_path.string());
        }

        target.clear();
        for (const auto& pair : root["strings"]) {
            target[pair.first.as<std::string>()] = pair.second.as<std::string>();
        }

        return {};
    } catch (const YAML::Exception& e) {
        return std::unexpected("Failed to parse YAML file '" + file_path.string() + "': " + e.what());
    }
}

void LocalizationManager::discoverLocales() {
    available_locales_.clear();

    if (!std::filesystem::exists(locales_dir_) || !std::filesystem::is_directory(locales_dir_)) {
        spdlog::warn("Locales directory not found: {}", locales_dir_.string());
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(locales_dir_)) {
        if (entry.path().extension() == ".yaml") {
            try {
                YAML::Node root = YAML::LoadFile(entry.path().string());
                auto code = root["locale"].as<std::string>();
                auto name = root["name"].as<std::string>();
                available_locales_.emplace_back(std::move(code), std::move(name));
            } catch (const YAML::Exception& e) {
                spdlog::warn("Failed to read locale file '{}': {}", entry.path().string(), e.what());
            }
        }
    }

    // Sort by locale code for consistent ordering
    std::ranges::sort(available_locales_, [](const auto& a, const auto& b) { return a.first < b.first; });

    spdlog::info("Discovered {} locale(s) in {}", available_locales_.size(), locales_dir_.string());
}

}  // namespace sudoku::core
