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

#include "di_container.h"

namespace sudoku::core {

namespace {
// Global container instance (file-local with internal linkage)
// Justification: Dependency injection container must be mutable to register/resolve dependencies
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
DIContainer g_container;
}  // anonymous namespace

DIContainer& getContainer() {
    return g_container;
}

}  // namespace sudoku::core