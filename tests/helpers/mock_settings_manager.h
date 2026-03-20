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

#include "core/i_settings_manager.h"

namespace sudoku::core {

/// In-memory mock for testing code that depends on ISettingsManager.
/// Stores settings in memory without file I/O.
class MockSettingsManager final : public ISettingsManager {
public:
    [[nodiscard]] const Settings& getSettings() const override {
        return settings_;
    }

    void setMaxHints(int value) override {
        updateIf(settings_.max_hints, value);
    }
    void setAutoSaveInterval(int ms) override {
        updateIf(settings_.auto_save_interval_ms, ms);
    }
    void setDefaultDifficulty(Difficulty d) override {
        updateIf(settings_.default_difficulty, d);
    }
    void setShowConflicts(bool v) override {
        updateIf(settings_.show_conflicts, v);
    }
    void setShowHints(bool v) override {
        updateIf(settings_.show_hints, v);
    }
    void setLanguage(std::string_view code) override {
        auto s = std::string(code);
        updateIf(settings_.language, s);
    }

    [[nodiscard]] Observable<Settings>& settingsObservable() override {
        return observable_;
    }

    // Test helpers
    Settings settings_;
    Observable<Settings> observable_{settings_};

private:
    template <typename T>
    void updateIf(T& field, const T& value) {
        if (field != value) {
            field = value;
            observable_.set(settings_);
        }
    }
};

}  // namespace sudoku::core
