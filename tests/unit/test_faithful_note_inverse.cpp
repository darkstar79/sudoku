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

// Story 6.1 (GH #76): cleanupConflictingNotes / restoreConflictingNotes were not faithful
// inverses, so pencil marks were silently corrupted on undo/clear. The fix records the exact
// peer-note delta in the core::Move and replays it verbatim on revert/redo. These tests pin
// the faithful round-trip: cleanup and restore are true inverses across PlaceNumber, PlaceHint
// and the clearCell (RemoveNumber) path, with no re-derivation against the live board.
//
// Optionals are extracted with value_or(<default>) after a has_value() REQUIRE: the value is
// always real, and value_or is an always-safe access so CI's bugprone-unchecked-optional-access
// gate (which does not model REQUIRE as a guard) stays happy.

#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

using NotesSnapshot = std::array<core::CellNotes, core::TOTAL_CELLS>;

/// Full-board pencil-mark snapshot, for exact (value-equal) round-trip assertions.
[[nodiscard]] NotesSnapshot snapshotNotes(const model::GameState& state) {
    NotesSnapshot snap;
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            snap[(row * core::BOARD_SIZE) + col] = state.getNotes(row, col);
        }
    }
    return snap;
}

[[nodiscard]] bool isPeer(const core::Position& a, const core::Position& b) {
    if (a == b) {
        return false;
    }
    if (a.row == b.row || a.col == b.col) {
        return true;
    }
    return (a.row / core::BOX_SIZE == b.row / core::BOX_SIZE) && (a.col / core::BOX_SIZE == b.col / core::BOX_SIZE);
}

struct ClearScenario {
    core::Position placement;  // empty cell we place into
    core::Position peer;       // empty peer that legitimately holds `value` as a candidate
    int value{0};
};

/// Find an (empty placement cell, empty peer, candidate value) triple where the peer holds the
/// value as a legal pencil mark — so placing it strips the note and clearing must restore it.
/// Requires notes to be populated (call fillNotes first).
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — exhaustive board scan; nesting is inherent
[[nodiscard]] std::optional<ClearScenario> findClearRestoreScenario(const model::GameState& state) {
    for (size_t pr = 0; pr < core::BOARD_SIZE; ++pr) {
        for (size_t pc = 0; pc < core::BOARD_SIZE; ++pc) {
            const core::Position placement{.row = pr, .col = pc};
            if (state.getValue(pr, pc) != core::EMPTY_CELL) {
                continue;
            }
            for (size_t qr = 0; qr < core::BOARD_SIZE; ++qr) {
                for (size_t qc = 0; qc < core::BOARD_SIZE; ++qc) {
                    const core::Position peer{.row = qr, .col = qc};
                    if (state.getValue(qr, qc) != core::EMPTY_CELL || !isPeer(placement, peer)) {
                        continue;
                    }
                    for (int v = core::MIN_VALUE; v <= core::MAX_VALUE; ++v) {
                        if (state.getNotes(peer).contains(v)) {
                            return ClearScenario{.placement = placement, .peer = peer, .value = v};
                        }
                    }
                }
            }
        }
    }
    return std::nullopt;
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("Faithful note inverse on undo/clear", "[game_view_model][undo][regression][bug-76-notes]") {
    test::GameViewModelFixture fixture;
    auto& vm = *fixture.view_model;

    SECTION("Undo of a placement does not over-restore notes to peers (AC#3)") {
        // A fresh board has NO pencil marks. Placing a value strips nothing (no peer holds it),
        // so undoing the placement must leave every peer exactly as it was — empty. The old
        // restoreConflictingNotes re-derived candidates and *added* the value to every legal
        // peer, inventing pencil marks that were never there.
        vm.startNewGame(Difficulty::Easy);
        const auto& state = vm.gameState.get();
        REQUIRE(state.hasSolution());

        const NotesSnapshot before = snapshotNotes(state);  // all empty

        const auto target = test::findEmptyCell(state);
        REQUIRE(target.has_value());
        const Position pos = target.value_or(Position{});
        const int value = state.getSolutionBoard()[pos.row][pos.col];

        vm.enterNumber(pos, value);
        vm.undo();

        REQUIRE(snapshotNotes(vm.gameState.get()) == before);
    }

    SECTION("Undo of a placement restores exactly the stripped candidates (AC#3 round-trip)") {
        vm.startNewGame(Difficulty::Easy);
        // Auto-fill every empty cell with its legal candidates, then round-trip a placement.
        vm.fillNotes();
        const auto& state = vm.gameState.get();

        const NotesSnapshot before = snapshotNotes(state);

        const auto target = test::findEmptyCell(state);
        REQUIRE(target.has_value());
        const Position pos = target.value_or(Position{});
        // A placement strips its own candidates from peers; placing a real candidate guarantees
        // there is something to strip and later restore.
        const int value = state.getSolutionBoard()[pos.row][pos.col];

        vm.enterNumber(pos, value);
        vm.undo();

        REQUIRE(snapshotNotes(vm.gameState.get()) == before);
    }

    SECTION("Manual elimination is not resurrected when an unrelated placement is undone (AC#7a)") {
        vm.startNewGame(Difficulty::Easy);
        const auto& state = vm.gameState.get();

        const auto pair = test::findTwoEmptyCellsInRow(state);
        REQUIRE(pair.has_value());
        const auto cells = pair.value_or(std::pair<Position, Position>{});
        const Position placement_cell = cells.first;
        const Position manual_cell = cells.second;  // a peer of placement_cell (same row)

        // The player has deliberately pencilled {keep_a,keep_b} into manual_cell and, crucially,
        // has NOT pencilled `value` there — that candidate was manually eliminated. A faithful
        // undo must never resurrect it.
        const int value = state.getSolutionBoard()[placement_cell.row][placement_cell.col];
        const int keep_a = (value % core::MAX_VALUE) + core::MIN_VALUE;
        const int keep_b = (keep_a % core::MAX_VALUE) + core::MIN_VALUE;
        core::CellNotes manual_notes;
        manual_notes.add(keep_a);
        manual_notes.add(keep_b);
        vm.gameState.update([&](model::GameState& s) { s.setNotes(manual_cell, manual_notes); });

        vm.enterNumber(placement_cell, value);
        vm.undo();

        REQUIRE(vm.gameState.get().getNotes(manual_cell) == manual_notes);
    }

    SECTION("Round-trip: undoing back to every index restores the notes snapshot (AC#7b)") {
        vm.startNewGame(Difficulty::Easy);
        const auto& state = vm.gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();

        const auto cells_opt = test::findEmptyCells(state, 4);
        REQUIRE(cells_opt.has_value());
        const auto cells = cells_opt.value_or(std::vector<Position>{});

        // Seed some partial, player-authored notes so over-restore (not just under-restore)
        // is exercised: arbitrary scratch marks that a candidate re-derivation would disturb.
        vm.gameState.update([&](model::GameState& s) {
            core::CellNotes scratch;
            scratch.add(2);
            s.setNotes(cells[3], scratch);
        });

        std::vector<NotesSnapshot> snapshots;
        snapshots.push_back(snapshotNotes(vm.gameState.get()));  // before move 0

        for (size_t i = 0; i < 3; ++i) {
            const Position pos = cells[i];
            vm.enterNumber(pos, solution[pos.row][pos.col]);
            snapshots.push_back(snapshotNotes(vm.gameState.get()));  // before move i+1
        }

        // Undo back to each index k and assert the live notes equal the pre-move-k snapshot.
        for (size_t k = 3; k-- > 0;) {
            vm.undo();
            REQUIRE(snapshotNotes(vm.gameState.get()) == snapshots[k]);
        }
    }

    SECTION("clearCell restores the peer notes the placement stripped (AC#4)") {
        vm.startNewGame(Difficulty::Easy);
        vm.fillNotes();  // every empty cell now carries its legal candidates

        const auto scenario_opt = findClearRestoreScenario(vm.gameState.get());
        REQUIRE(scenario_opt.has_value());
        const auto scenario = scenario_opt.value_or(ClearScenario{});
        const Position placement_cell = scenario.placement;
        const Position peer = scenario.peer;
        const int value = scenario.value;

        vm.enterNumber(placement_cell, value);
        REQUIRE(!vm.gameState.get().getNotes(peer).contains(value));  // stripped

        // Clearing the placement must give the candidate back (old clearCell did nothing).
        vm.clearCell(placement_cell);
        REQUIRE(vm.gameState.get().getNotes(peer).contains(value));
    }

    SECTION("Redo replays the recorded delta, not a re-derivation (AC#6)") {
        vm.startNewGame(Difficulty::Easy);
        vm.fillNotes();
        const auto& state = vm.gameState.get();

        const auto target = test::findEmptyCell(state);
        REQUIRE(target.has_value());
        const Position pos = target.value_or(Position{});
        const int value = state.getSolutionBoard()[pos.row][pos.col];

        vm.enterNumber(pos, value);
        const NotesSnapshot after_place = snapshotNotes(vm.gameState.get());

        vm.undo();
        vm.redo();

        // Redo must reproduce the exact post-placement notes via the stored delta.
        REQUIRE(snapshotNotes(vm.gameState.get()) == after_place);
    }
}
