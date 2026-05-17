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

#include <algorithm>
#include <string_view>
#include <system_error>

namespace sudoku::core {

namespace {

constexpr std::string_view kFilenamePrefix = "sudoku_";
constexpr std::string_view kFilenameSuffix = ".qm";

[[nodiscard]] constexpr bool isLower(char c) noexcept {
    return c >= 'a' && c <= 'z';
}

[[nodiscard]] constexpr bool isUpper(char c) noexcept {
    return c >= 'A' && c <= 'Z';
}

}  // namespace

bool isValidLocaleCode(std::string_view code) noexcept {
    if (code.empty()) {
        return false;
    }

    size_t i = 0;
    while (i < code.size() && isLower(code[i])) {
        ++i;
    }
    if (i < 2 || i > 3) {
        return false;
    }

    if (i == code.size()) {
        return true;
    }

    if (code[i] != '_') {
        return false;
    }
    ++i;

    const size_t region_start = i;
    while (i < code.size() && isUpper(code[i])) {
        ++i;
    }
    if (i - region_start != 2) {
        return false;
    }

    return i == code.size();
}

std::vector<std::string> scanInstalledLocales(const std::filesystem::path& translations_dir) {
    std::vector<std::string> result;

    std::error_code ec;
    if (!std::filesystem::is_directory(translations_dir, ec)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(translations_dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.size() <= kFilenamePrefix.size() + kFilenameSuffix.size()) {
            continue;
        }
        if (!std::string_view{name}.starts_with(kFilenamePrefix)) {
            continue;
        }
        if (!std::string_view{name}.ends_with(kFilenameSuffix)) {
            continue;
        }
        std::string code =
            name.substr(kFilenamePrefix.size(), name.size() - kFilenamePrefix.size() - kFilenameSuffix.size());
        if (!isValidLocaleCode(code)) {
            continue;
        }
        result.push_back(std::move(code));
    }

    std::ranges::sort(result);
    const auto trim = std::ranges::unique(result);
    result.erase(trim.begin(), trim.end());
    return result;
}

}  // namespace sudoku::core
