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

#include "observable.h"
#include "settings.h"

#include <string_view>

namespace sudoku::core {

/// Interface for managing persistent application settings.
///
/// Production code uses SettingsManager (YAML file persistence).
/// Test code uses MockSettingsManager (in-memory defaults).
class ISettingsManager {
public:
    virtual ~ISettingsManager() = default;

    /// Get the current settings snapshot.
    [[nodiscard]] virtual const Settings& getSettings() const = 0;

    // Typed setters — each persists immediately and notifies observers if changed.

    virtual void setMaxHints(int value) = 0;
    virtual void setAutoSaveInterval(int ms) = 0;
    virtual void setDefaultDifficulty(Difficulty difficulty) = 0;
    virtual void setShowConflicts(bool value) = 0;
    virtual void setShowHints(bool value) = 0;
    virtual void setShowSessionTimer(bool value) = 0;
    virtual void setCollectDetailedStats(bool value) = 0;
    virtual void setEncryptDetailedStats(bool value) = 0;
    virtual void setLanguage(std::string_view locale_code) = 0;
    virtual void setExperimentalTrainingMode(bool value) = 0;
    virtual void setExperimentalCoachingHints(bool value) = 0;

    /// Observable for reactive UI updates when settings change.
    [[nodiscard]] virtual Observable<Settings>& settingsObservable() = 0;

protected:
    ISettingsManager() = default;
    ISettingsManager(const ISettingsManager&) = default;
    ISettingsManager& operator=(const ISettingsManager&) = default;
    ISettingsManager(ISettingsManager&&) = default;
    ISettingsManager& operator=(ISettingsManager&&) = default;
};

}  // namespace sudoku::core
