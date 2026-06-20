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

#include "settings_manager.h"

#include "infrastructure/app_directory_manager.h"
#include "locale_utils.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sudoku::core {

namespace {

[[nodiscard]] constexpr std::string_view difficultyToString(Difficulty d) {
    switch (d) {
        case Difficulty::Easy:
            return "Easy";
        case Difficulty::Medium:
            return "Medium";
        case Difficulty::Hard:
            return "Hard";
        case Difficulty::Expert:
            return "Expert";
        case Difficulty::Master:
            return "Master";
        default:
            return "Medium";
    }
}

// Whole-day ceiling for any play-limit minute count. 0 is allowed (an enabled limit with 0 minutes
// reads as unlimited — the policy guards minutes <= 0).
constexpr int kMaxLimitMinutes = 1440;

void loadTimeLimits(const YAML::Node& root, Settings& settings) {
    auto time_limits = root["time_limits"];
    if (!time_limits) {
        return;
    }
    if (auto v = time_limits["enable_session_limit"]) {
        settings.enable_session_limit = v.as<bool>();
    }
    if (auto v = time_limits["max_session_minutes"]) {
        settings.max_session_minutes = std::clamp(v.as<int>(), 0, kMaxLimitMinutes);
    }
    if (auto v = time_limits["session_cooldown_minutes"]) {
        settings.session_cooldown_minutes = std::clamp(v.as<int>(), 0, kMaxLimitMinutes);
    }
    if (auto v = time_limits["enable_daily_limit"]) {
        settings.enable_daily_limit = v.as<bool>();
    }
    if (auto v = time_limits["max_daily_minutes"]) {
        settings.max_daily_minutes = std::clamp(v.as<int>(), 0, kMaxLimitMinutes);
    }
    if (auto v = time_limits["warn_before_minutes"]) {
        settings.warn_before_minutes = std::clamp(v.as<int>(), 0, kMaxLimitMinutes);
    }
}

void loadLanguageField(const YAML::Node& root, Settings& settings, const std::filesystem::path& path) {
    auto v = root["language"];
    if (!v) {
        return;
    }
    auto code = v.as<std::string>();
    if (isValidLocaleCode(code)) {
        settings.language = std::move(code);
    } else {
        spdlog::warn("Ignoring invalid 'language' value '{}' in {}; keeping default '{}'", code, path.string(),
                     settings.language);
    }
}

[[nodiscard]] constexpr Difficulty difficultyFromString(std::string_view s) {
    if (s == "Easy") {
        return Difficulty::Easy;
    }
    if (s == "Medium") {
        return Difficulty::Medium;
    }
    if (s == "Hard") {
        return Difficulty::Hard;
    }
    if (s == "Expert") {
        return Difficulty::Expert;
    }
    if (s == "Master") {
        return Difficulty::Master;
    }
    return Difficulty::Medium;
}

}  // namespace

SettingsManager::SettingsManager(std::filesystem::path settings_path) : settings_path_(std::move(settings_path)) {
    migrateLanguageFile();
    load();
}

SettingsManager::SettingsManager()
    : SettingsManager(
          infrastructure::AppDirectoryManager::getDefaultDirectory(infrastructure::DirectoryType::Saves).parent_path() /
          "settings.yaml") {
}

const Settings& SettingsManager::getSettings() const {
    return settings_;
}

void SettingsManager::setMaxHints(int value) {
    value = std::clamp(value, 1, 50);
    auto old = settings_;
    settings_.max_hints = value;
    notifyIfChanged(old);
}

void SettingsManager::setAutoSaveInterval(int ms) {
    ms = std::clamp(ms, 10000, 300000);
    auto old = settings_;
    settings_.auto_save_interval_ms = ms;
    notifyIfChanged(old);
}

void SettingsManager::setDefaultDifficulty(Difficulty difficulty) {
    auto old = settings_;
    settings_.default_difficulty = difficulty;
    notifyIfChanged(old);
}

void SettingsManager::setShowConflicts(bool value) {
    auto old = settings_;
    settings_.show_conflicts = value;
    notifyIfChanged(old);
}

void SettingsManager::setShowHints(bool value) {
    auto old = settings_;
    settings_.show_hints = value;
    notifyIfChanged(old);
}

void SettingsManager::setShowSessionTimer(bool value) {
    auto old = settings_;
    settings_.show_session_timer = value;
    notifyIfChanged(old);
}

void SettingsManager::setCollectDetailedStats(bool value) {
    auto old = settings_;
    settings_.collect_detailed_stats = value;
    notifyIfChanged(old);
}

void SettingsManager::setEncryptDetailedStats(bool value) {
    auto old = settings_;
    settings_.encrypt_detailed_stats = value;
    notifyIfChanged(old);
}

void SettingsManager::setLanguage(std::string_view locale_code) {
    if (!isValidLocaleCode(locale_code)) {
        spdlog::warn("Rejected invalid locale code '{}' from setLanguage()", locale_code);
        return;
    }
    auto old = settings_;
    settings_.language = std::string(locale_code);
    notifyIfChanged(old);
}

void SettingsManager::setExperimentalTrainingMode(bool value) {
    auto old = settings_;
    settings_.experimental_training_mode = value;
    notifyIfChanged(old);
}

void SettingsManager::setExperimentalCoachingHints(bool value) {
    auto old = settings_;
    settings_.experimental_coaching_hints = value;
    notifyIfChanged(old);
}

void SettingsManager::setEnableSessionLimit(bool value) {
    auto old = settings_;
    settings_.enable_session_limit = value;
    notifyIfChanged(old);
}

void SettingsManager::setMaxSessionMinutes(int minutes) {
    auto old = settings_;
    settings_.max_session_minutes = std::clamp(minutes, 0, kMaxLimitMinutes);
    notifyIfChanged(old);
}

void SettingsManager::setSessionCooldownMinutes(int minutes) {
    auto old = settings_;
    settings_.session_cooldown_minutes = std::clamp(minutes, 0, kMaxLimitMinutes);
    notifyIfChanged(old);
}

void SettingsManager::setEnableDailyLimit(bool value) {
    auto old = settings_;
    settings_.enable_daily_limit = value;
    notifyIfChanged(old);
}

void SettingsManager::setMaxDailyMinutes(int minutes) {
    auto old = settings_;
    settings_.max_daily_minutes = std::clamp(minutes, 0, kMaxLimitMinutes);
    notifyIfChanged(old);
}

void SettingsManager::setWarnBeforeMinutes(int minutes) {
    auto old = settings_;
    settings_.warn_before_minutes = std::clamp(minutes, 0, kMaxLimitMinutes);
    notifyIfChanged(old);
}

Observable<Settings>& SettingsManager::settingsObservable() {
    return observable_;
}

void SettingsManager::notifyIfChanged(const Settings& old_settings) {
    if (settings_ != old_settings) {
        save();
        observable_.set(settings_);
    }
}

void SettingsManager::load() {
    if (!std::filesystem::exists(settings_path_)) {
        return;  // Use defaults
    }

    try {
        auto root = YAML::LoadFile(settings_path_.string());

        if (auto gameplay = root["gameplay"]) {
            if (auto v = gameplay["max_hints"]) {
                settings_.max_hints = std::clamp(v.as<int>(), 1, 50);
            }
            if (auto v = gameplay["auto_save_interval_ms"]) {
                settings_.auto_save_interval_ms = std::clamp(v.as<int>(), 10000, 300000);
            }
            if (auto v = gameplay["default_difficulty"]) {
                settings_.default_difficulty = difficultyFromString(v.as<std::string>());
            }
        }

        if (auto display = root["display"]) {
            if (auto v = display["show_conflicts"]) {
                settings_.show_conflicts = v.as<bool>();
            }
            if (auto v = display["show_hints"]) {
                settings_.show_hints = v.as<bool>();
            }
            if (auto v = display["show_session_timer"]) {
                settings_.show_session_timer = v.as<bool>();
            }
            if (auto v = display["collect_detailed_stats"]) {
                settings_.collect_detailed_stats = v.as<bool>();
            }
            if (auto v = display["encrypt_detailed_stats"]) {
                settings_.encrypt_detailed_stats = v.as<bool>();
            }
            // auto_notes_on_startup: ignored (feature removed)
        }

        loadTimeLimits(root, settings_);

        loadLanguageField(root, settings_, settings_path_);

        if (auto experimental = root["experimental"]) {
            if (auto v = experimental["training_mode"]) {
                settings_.experimental_training_mode = v.as<bool>();
            }
            if (auto v = experimental["coaching_hints"]) {
                settings_.experimental_coaching_hints = v.as<bool>();
            }
        }

        observable_.set(settings_);
        spdlog::debug("Settings loaded from {}", settings_path_.string());
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load settings from {}: {}. Using defaults.", settings_path_.string(), e.what());
        settings_ = Settings{};
    }
}

void SettingsManager::save() const {
    try {
        std::filesystem::create_directories(settings_path_.parent_path());

        YAML::Node root;

        YAML::Node gameplay;
        gameplay["max_hints"] = settings_.max_hints;
        gameplay["auto_save_interval_ms"] = settings_.auto_save_interval_ms;
        gameplay["default_difficulty"] = std::string(difficultyToString(settings_.default_difficulty));
        root["gameplay"] = gameplay;

        YAML::Node display;
        display["show_conflicts"] = settings_.show_conflicts;
        display["show_hints"] = settings_.show_hints;
        display["show_session_timer"] = settings_.show_session_timer;
        display["collect_detailed_stats"] = settings_.collect_detailed_stats;
        display["encrypt_detailed_stats"] = settings_.encrypt_detailed_stats;
        root["display"] = display;

        YAML::Node time_limits;
        time_limits["enable_session_limit"] = settings_.enable_session_limit;
        time_limits["max_session_minutes"] = settings_.max_session_minutes;
        time_limits["session_cooldown_minutes"] = settings_.session_cooldown_minutes;
        time_limits["enable_daily_limit"] = settings_.enable_daily_limit;
        time_limits["max_daily_minutes"] = settings_.max_daily_minutes;
        time_limits["warn_before_minutes"] = settings_.warn_before_minutes;
        root["time_limits"] = time_limits;

        root["language"] = settings_.language;

        YAML::Node experimental;
        experimental["training_mode"] = settings_.experimental_training_mode;
        experimental["coaching_hints"] = settings_.experimental_coaching_hints;
        root["experimental"] = experimental;

        std::ofstream file(settings_path_);
        if (!file.is_open()) {
            spdlog::error("Failed to open settings file for writing: {}", settings_path_.string());
            return;
        }
        file << root;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save settings to {}: {}", settings_path_.string(), e.what());
    }
}

void SettingsManager::migrateLanguageFile() {
    if (std::filesystem::exists(settings_path_)) {
        return;  // Already have settings file, no migration needed
    }

    auto language_file = settings_path_.parent_path() / "language.txt";
    if (!std::filesystem::exists(language_file)) {
        return;  // No legacy file either
    }

    std::ifstream in(language_file);
    std::string locale_code;
    if (in && std::getline(in, locale_code) && !locale_code.empty()) {
        if (!isValidLocaleCode(locale_code)) {
            spdlog::warn("Ignoring invalid locale code '{}' in legacy language.txt", locale_code);
            return;
        }
        settings_.language = locale_code;
        save();
        spdlog::info("Migrated language preference '{}' from language.txt to settings.yaml", locale_code);
    }
}

}  // namespace sudoku::core
