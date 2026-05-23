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
#include <string_view>

namespace sudoku::core {

/// Abstraction over the system clipboard.
///
/// Production (`infrastructure::QtClipboardProvider`) wraps QApplication::clipboard().
/// Tests use `InMemoryClipboardProvider` (see `tests/helpers/`) which is fully
/// deterministic and headless-safe.
class IClipboardProvider {
public:
    virtual ~IClipboardProvider() = default;

    /// Sets the clipboard's text contents.
    virtual void setText(std::string_view text) = 0;

    /// Reads the current clipboard text. Returns an empty string when the clipboard is empty
    /// or holds a non-text mime type.
    [[nodiscard]] virtual std::string getText() const = 0;

protected:
    IClipboardProvider() = default;
    IClipboardProvider(const IClipboardProvider&) = default;
    IClipboardProvider& operator=(const IClipboardProvider&) = default;
    IClipboardProvider(IClipboardProvider&&) = default;
    IClipboardProvider& operator=(IClipboardProvider&&) = default;
};

}  // namespace sudoku::core
