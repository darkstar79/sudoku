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

#include <string>
#include <utility>

#include <QCoreApplication>
#include <fmt/format.h>

namespace sudoku::core {

[[nodiscard]] inline std::string loc(const char* context, const char* source) {
    return QCoreApplication::translate(context, source).toStdString();
}

// locFormat takes an already-translated string (typically from core::loc) and
// runs it through fmt::format. The split exists so lupdate sees translatable
// literals through the 2-arg `core::loc(...)` calls — which match its
// `translate(ctx, src)` alias — rather than buried inside a variadic template
// it can't reliably destructure.
//
//   core::locFormat(core::loc("Sudoku", "Score: {0}"), score)
template <typename... Args>
[[nodiscard]] std::string locFormat(const std::string& translated, Args&&... args) {
    return fmt::format(fmt::runtime(translated), std::forward<Args>(args)...);
}

// Block the legacy 3-arg form `locFormat("Sudoku", "fmt {0}", v)`. Without this
// deleted overload, `const char* "Sudoku"` would implicitly convert to the
// std::string parameter above, fmt::format would run on "Sudoku" (zero format
// specifiers), and the rest of the args would be silently discarded — a hidden
// runtime regression with no compile-time warning.
template <typename... Args>
std::string locFormat(const char*, Args&&...) = delete;

}  // namespace sudoku::core
