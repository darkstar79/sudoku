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

namespace sudoku::core {

/// Version of the puzzle rating model. Persisted into saves (SavedGame::rating_model_version)
/// so a stored rating can be recognized later as the product of an older model once the model
/// evolves — the firewall that keeps a saved game from being silently reclassified.
///
/// BUMP THIS whenever the rating model changes in a way that could alter a puzzle's computed
/// rating, difficulty bucket, or technique set — specifically on ANY of:
///   - a technique-SET change (techniques added to / removed from SolvingTechnique), OR
///   - a rating-VALUE change (any getTechniqueRating return value), OR
///   - a pathfinding / step-ordering / minimization change in the rater.
/// It is NOT value-only: a set or ordering change with no single value edit still changes
/// results and still requires a bump.
///
/// On load, a save whose rating_model_version differs from this constant keeps its stored rating
/// literals (snapshot-preserve; never recomputed — the per-step context needed to re-rate is not
/// stored) and reports SavedGame::isRatingStale().
///
/// Version history:
///   1 — pre-1.0.0 baseline (0b.0).
///   2 — 0b.3 flat corrections: Empty Rectangle 4.5→4.3, ALS-XZ 7.5→6.8, Sue de Coq 7.5→6.6.
///       Three getTechniqueRating() return values changed, so saves rated under v1 are now stale.
///
/// Lives in this tiny dependency-free header (not puzzle_rating.h) so SavedGame can compute
/// staleness without pulling the rating/solver headers into every i_save_manager.h consumer.
inline constexpr int RATING_MODEL_VERSION = 2;

}  // namespace sudoku::core
