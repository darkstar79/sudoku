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
/// PRE-1.0.0 CARVE-OUT: the bump rule above is a POST-TAG obligation (it protects shipped saves).
/// Before the 1.0.0 tag there are no shipped saves to protect, so the 0b.4 a/b/c context-sensitive
/// rating work intentionally changes computed ratings WITHOUT bumping — the per-cycle artifact is the
/// golden-pin re-base, not a version increment. The version resumes bumping for any post-1.0.0 change.
///

/// On load, a save whose rating_model_version differs from this constant keeps its stored rating
/// literals (snapshot-preserve; never recomputed — the per-step context needed to re-rate is not
/// stored) and reports SavedGame::isRatingStale().
///
/// Version history:
///   1 — pre-1.0.0 baseline (0b.0).
///   2 — 0b.3 flat corrections: Empty Rectangle 4.5→4.3, ALS-XZ 7.5→6.8, Sue de Coq 7.5→6.6.
///       Three getTechniqueRating() return values changed, so saves rated under v1 are now stale.
///       (Stays 2 through 0b.4a: Hidden Single block 1.2 / line 1.5 context split — pre-1.0.0
///       carve-out above; new computed rating value, deliberately NO bump.)
///
/// Lives in this tiny dependency-free header (not puzzle_rating.h) so SavedGame can compute
/// staleness without pulling the rating/solver headers into every i_save_manager.h consumer.
inline constexpr int RATING_MODEL_VERSION = 2;

}  // namespace sudoku::core
