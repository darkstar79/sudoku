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

#include "locale_utils.h"

#include "../core/locale_utils.h"

#include <algorithm>

#include <QCoreApplication>
#include <QLocale>

namespace sudoku::view {

std::filesystem::path findTranslationsDir() {
    const auto exe_dir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString());
    for (const auto& candidate : {exe_dir / "translations", exe_dir / ".." / "share" / "sudoku" / "translations"}) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

std::vector<LocaleEntry> discoverInstalledLocales() {
    std::vector<std::string> codes;
    if (auto dir = findTranslationsDir(); !dir.empty()) {
        codes = core::scanInstalledLocales(dir);
    }

    // "en" is the source language: even with no .qm files, the UI still works
    // in English. Guarantee it appears in the combo.
    if (std::find(codes.begin(), codes.end(), "en") == codes.end()) {
        codes.emplace_back("en");
        std::sort(codes.begin(), codes.end());
    }

    std::vector<LocaleEntry> entries;
    entries.reserve(codes.size());
    for (auto& code : codes) {
        QLocale qlocale(QString::fromStdString(code));
        QString display = qlocale.nativeLanguageName();
        if (display.isEmpty()) {
            display = QString::fromStdString(code);
        }
        entries.push_back({std::move(code), std::move(display)});
    }
    return entries;
}

}  // namespace sudoku::view
