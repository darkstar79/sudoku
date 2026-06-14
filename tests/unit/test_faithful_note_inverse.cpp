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
#include <catch2/generators/catch_generators.hpp>

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

    SECTION("Clearing a placement does not resurrect a manually-eliminated candidate (AC#4 / hybrid Option 4)") {
        // The faithful clear must reuse the placement's actual-removals delta, NOT re-derive
        // "every legal empty peer" — otherwise it re-adds the value to a peer the player
        // deliberately eliminated. Set up a placement cell C and two empty row-peers: P legitimately
        // holds the value, Q has it manually eliminated.
        vm.startNewGame(Difficulty::Easy);
        const auto& state = vm.gameState.get();
        REQUIRE(state.hasSolution());

        std::optional<std::array<Position, 3>> picked;
        for (size_t row = 0; row < core::BOARD_SIZE && !picked.has_value(); ++row) {
            std::vector<Position> empties;
            for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
                if (state.getValue(row, col) == core::EMPTY_CELL) {
                    empties.push_back(Position{.row = row, .col = col});
                }
            }
            if (empties.size() >= 3) {
                picked = std::array<Position, 3>{empties[0], empties[1], empties[2]};
            }
        }
        REQUIRE(picked.has_value());
        const auto cells = picked.value_or(std::array<Position, 3>{});
        const Position c = cells[0];  // placement
        const Position p = cells[1];  // peer that legitimately holds the value
        const Position q = cells[2];  // peer where the value was manually eliminated
        const int value = state.getSolutionBoard()[c.row][c.col];
        const int other = (value % core::MAX_VALUE) + core::MIN_VALUE;  // never == value

        core::CellNotes p_notes;
        p_notes.add(value);
        core::CellNotes q_notes;
        q_notes.add(other);  // Q deliberately does NOT carry `value`
        vm.gameState.update([&](model::GameState& s) {
            s.setNotes(p, p_notes);
            s.setNotes(q, q_notes);
        });

        vm.enterNumber(c, value);
        REQUIRE(!vm.gameState.get().getNotes(p).contains(value));  // placement stripped it from P

        vm.clearCell(c);

        // Faithful: P regains the value (it was in the placement delta); Q is untouched.
        REQUIRE(vm.gameState.get().getNotes(p).contains(value));
        REQUIRE(!vm.gameState.get().getNotes(q).contains(value));
    }

    SECTION("fillNotes drops a pending redo so replay cannot use a stale delta (redo-tail guard)") {
        // fillNotes/clearAllNotes rewrite pencil marks outside the move model. A pending redo would
        // replay its stored delta against the rewritten notes — so the redo tail must be invalidated.
        vm.startNewGame(Difficulty::Easy);
        const auto& state = vm.gameState.get();

        const auto target = test::findEmptyCell(state);
        REQUIRE(target.has_value());
        const Position pos = target.value_or(Position{});
        const int value = state.getSolutionBoard()[pos.row][pos.col];

        vm.enterNumber(pos, value);
        vm.undo();
        REQUIRE(vm.canRedo());

        vm.fillNotes();

        REQUIRE(!vm.canRedo());
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

// AC#7b general round-trip property: a seeded sequence of RECORDED moves — placements, note
// add/remove (enterNote toggles), and a clear — undone back to every index must reproduce the
// exact NotesData snapshot taken before that index, down to the empty-grid base case (k=0).
// GENERATE over a fixed seed list keeps failures deterministically reproducible.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("Faithful note inverse — seeded round-trip property (AC#7b)",
          "[game_view_model][undo][regression][bug-76-notes]") {
    const unsigned seed = GENERATE(1u, 2u, 3u, 4u);
    test::GameViewModelFixture fixture;
    auto& vm = *fixture.view_model;

    vm.startNewGame(Difficulty::Easy);
    const auto& state = vm.gameState.get();
    REQUIRE(state.hasSolution());
    const auto& solution = state.getSolutionBoard();

    const auto cells_opt = test::findEmptyCells(state, 6);
    REQUIRE(cells_opt.has_value());
    const auto cells = cells_opt.value_or(std::vector<Position>{});

    // Seed-varied note digits (any 1..9 is a legal pencil mark); the cell roles are fixed so the
    // move script stays valid (note cells stay empty until cleared/placed).
    const int d1 = static_cast<int>(seed % core::MAX_VALUE) + core::MIN_VALUE;
    const int d2 = static_cast<int>((seed + 3) % core::MAX_VALUE) + core::MIN_VALUE;

    std::vector<NotesSnapshot> snapshots;
    auto record = [&]() { snapshots.push_back(snapshotNotes(vm.gameState.get())); };

    // An interleaving of recorded moves: AddNote, AddNote, placement (may strip peer notes),
    // RemoveNote/AddNote toggle, placement, clear. Each is snapshotted before it runs.
    record();
    vm.enterNote(cells[0], d1);
    record();
    vm.enterNote(cells[1], d2);
    record();
    vm.enterNumber(cells[2], solution[cells[2].row][cells[2].col]);
    record();
    vm.enterNote(cells[0], d1);  // toggles the cells[0]/d1 mark (Remove or Add depending on strip)
    record();
    vm.enterNumber(cells[3], solution[cells[3].row][cells[3].col]);
    record();
    vm.clearCell(cells[3]);
    // snapshots now holds the pre-state of each of the 6 moves (indices 0..5).

    // Undo each move and assert the live notes equal the pre-move snapshot, down to k = 0
    // (the empty-grid base case is the final canary).
    for (size_t k = snapshots.size(); k-- > 0;) {
        vm.undo();
        REQUIRE(snapshotNotes(vm.gameState.get()) == snapshots[k]);
    }
}
