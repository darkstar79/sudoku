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

#include "i_settings_manager.h"

#include <filesystem>

namespace sudoku::core {

/// Persistent settings manager backed by a YAML file.
///
/// Loads settings from disk on construction (falls back to defaults on error).
/// Each setter persists immediately and notifies observers only if the value changed.
class SettingsManager final : public ISettingsManager {
public:
    /// Construct with explicit file path (for testability).
    explicit SettingsManager(std::filesystem::path settings_path);

    /// Construct with default platform path (~/.local/share/sudoku/settings.yaml).
    SettingsManager();

    [[nodiscard]] const Settings& getSettings() const override;

    void setMaxHints(int value) override;
    void setAutoSaveInterval(int ms) override;
    void setDefaultDifficulty(Difficulty difficulty) override;
    void setShowConflicts(bool value) override;
    void setShowHints(bool value) override;
    void setCollectDetailedStats(bool value) override;
    void setEncryptDetailedStats(bool value) override;
    void setLanguage(std::string_view locale_code) override;
    void setExperimentalTrainingMode(bool value) override;
    void setExperimentalCoachingHints(bool value) override;

    [[nodiscard]] Observable<Settings>& settingsObservable() override;

private:
    void load();
    void save() const;
    void notifyIfChanged(const Settings& old_settings);

    /// Migrate language.txt to settings.yaml on first run.
    void migrateLanguageFile();

    std::filesystem::path settings_path_;
    Settings settings_;
    Observable<Settings> observable_{settings_};
};

}  // namespace sudoku::core
