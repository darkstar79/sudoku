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

// Story 0b.0 — rating-model versioning & save-compat firewall.
// THE backstop for 0b.3/0b.4: proves stored rating literals survive a rating-model change
// (snapshot-preserve, never recomputed) and that the schema/rating versions round-trip.
// Sibling to test_save_origin_integration.cpp; mirrors its hand-crafted-YAML fixture pattern.

#include "../../src/core/puzzle_rating.h"  // RATING_MODEL_VERSION
#include "../../src/core/save_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
namespace fs = std::filesystem;

namespace {

class RatingModelCompatFixture {
public:
    RatingModelCompatFixture()
        : test_dir_("./test_rating_model_" +
                    std::to_string(std::chrono::system_clock::now().time_since_epoch().count())),
          save_manager_(test_dir_) {
        fs::create_directories(test_dir_);
    }

    ~RatingModelCompatFixture() {
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    RatingModelCompatFixture(const RatingModelCompatFixture&) = delete;
    RatingModelCompatFixture& operator=(const RatingModelCompatFixture&) = delete;
    RatingModelCompatFixture(RatingModelCompatFixture&&) = delete;
    RatingModelCompatFixture& operator=(RatingModelCompatFixture&&) = delete;

    // Drops a raw YAML save in the fixture dir under the given id; returns the load result.
    [[nodiscard]] std::expected<SavedGame, SaveError> loadRawYaml(const std::string& save_id,
                                                                  const std::string& yaml) const {
        auto save_path = fs::path(test_dir_) / (save_id + ".yaml");
        std::ofstream out(save_path, std::ios::binary);
        REQUIRE(out.is_open());
        out << yaml;
        out.close();
        return save_manager_.loadGame(save_id);
    }

    std::string test_dir_;
    SaveManager save_manager_;
};

// Fixtures are full `constexpr const char*` raw literals (mirroring test_save_origin_integration's
// pattern) rather than runtime-concatenated std::strings — that keeps them out of the
// throwing-static-initialization / cert-err58 path under the root clang-tidy config. The board grid
// is the canonical "5,3,0..." puzzle, repeated verbatim per fixture.

// Golden 1.0 save — predates BOTH the origin field AND the rating-model version hook.
// Missing keys must default (save_format_version → "1.0", rating_model_version → 0).
constexpr const char* kGolden_1_0 = R"(version: "1.0"
save_id: "golden_1_0"
display_name: "Golden 1.0 save"
created_time: 1747350000
last_modified: 1747350000
puzzle_data:
  difficulty: 0
  puzzle_seed: 42
  original_puzzle:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
  current_state:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
progress:
  elapsed_time: 12345
  moves_made: 0
  hints_used: 0
  mistakes: 0
  is_complete: false
)";

// Golden 1.1 save written under the current rating model — carries the new
// rating_model_version key equal to the current constant, plus a stored rating literal.
constexpr const char* kGolden_1_1 = R"(version: "1.1"
save_id: "golden_1_1"
display_name: "Golden 1.1 save"
created_time: 1747350000
last_modified: 1747350000
rating_model_version: 1
puzzle_data:
  difficulty: 2
  puzzle_seed: 42
  origin: 0
  original_puzzle:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
  current_state:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
progress:
  elapsed_time: 12345
  moves_made: 0
  hints_used: 0
  mistakes: 0
  is_complete: false
puzzle_rating: 4.5
puzzle_requires_backtracking: false
puzzle_technique_ids:
  - 0
  - 1
  - 11
)";

// Snapshot-invariant fixture: rating_model_version is deliberately OLDER than the current
// model (0 < current), and the stored puzzle_rating (9.99) is a value today's model would
// never compute for this board. Load MUST surface the stored 9.99, not a recompute, and
// flag the save stale.
constexpr const char* kStaleRatingSave = R"(version: "1.1"
save_id: "stale_rating"
display_name: "Stale-rating save"
created_time: 1747350000
last_modified: 1747350000
rating_model_version: 0
puzzle_data:
  difficulty: 4
  puzzle_seed: 42
  origin: 0
  original_puzzle:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
  current_state:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
progress:
  elapsed_time: 12345
  moves_made: 0
  hints_used: 0
  mistakes: 0
  is_complete: false
puzzle_rating: 9.99
puzzle_requires_backtracking: false
)";

// Unknown future schema version — must hit a defined best-effort branch, never UB / hard reject.
constexpr const char* kUnknownVersionSave = R"(version: "9.9"
save_id: "future_schema"
display_name: "Future-schema save"
created_time: 1747350000
last_modified: 1747350000
puzzle_data:
  difficulty: 0
  puzzle_seed: 42
  original_puzzle:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
  current_state:
    - [5, 3, 0, 0, 7, 0, 0, 0, 0]
    - [6, 0, 0, 1, 9, 5, 0, 0, 0]
    - [0, 9, 8, 0, 0, 0, 0, 6, 0]
    - [8, 0, 0, 0, 6, 0, 0, 0, 3]
    - [4, 0, 0, 8, 0, 3, 0, 0, 1]
    - [7, 0, 0, 0, 2, 0, 0, 0, 6]
    - [0, 6, 0, 0, 0, 0, 2, 8, 0]
    - [0, 0, 0, 4, 1, 9, 0, 0, 5]
    - [0, 0, 0, 0, 8, 0, 0, 7, 9]
progress:
  elapsed_time: 12345
  moves_made: 0
  hints_used: 0
  mistakes: 0
  is_complete: false
)";

SavedGame makeRatedGame(const std::string& id) {
    SavedGame game;
    game.save_id = id;
    game.display_name = "Round-trip game";
    game.original_puzzle = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                            {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                            {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};
    game.current_state = game.original_puzzle;
    game.difficulty = Difficulty::Hard;
    game.puzzle_rating = 4.5;
    game.puzzle_technique_ids = {0, 1, 11};
    // A freshly-rated game carries the current model version (producers stamp this; the default
    // member value is 0 = legacy). Without it the serializer would persist 0 and the round-trip
    // would read back a "stale" game.
    game.rating_model_version = RATING_MODEL_VERSION;
    return game;
}

}  // namespace

// ── AC1: loader reads the schema version (kills the dead write) ──────────────────────────

TEST_CASE("Loading a 1.1 save exposes save_format_version == \"1.1\"",
          "[integration][save_manager][rating_model][version]") {
    RatingModelCompatFixture fixture;

    auto loaded = fixture.loadRawYaml("golden_1_1", kGolden_1_1);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->save_format_version == "1.1");
}

TEST_CASE("Loading a 1.0 save with no version-bearing keys defaults save_format_version to \"1.0\"",
          "[integration][save_manager][rating_model][version]") {
    RatingModelCompatFixture fixture;

    auto loaded = fixture.loadRawYaml("golden_1_0", kGolden_1_0);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->save_format_version == "1.0");
}

// ── AC2: dedicated rating_model_version, round-trips; legacy → 0 ─────────────────────────

TEST_CASE("Saving then loading a game round-trips rating_model_version to the current model version",
          "[integration][save_manager][rating_model]") {
    RatingModelCompatFixture fixture;

    SaveSettings settings;
    settings.compress = false;
    settings.encrypt = false;

    auto save_result = fixture.save_manager_.saveGame(makeRatedGame("roundtrip"), settings);
    REQUIRE(save_result.has_value());
    auto loaded = fixture.save_manager_.loadGame(*save_result);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->rating_model_version == RATING_MODEL_VERSION);
    REQUIRE(loaded->rating_model_version == 1);
    // A freshly-rated, just-saved game must NOT come back stale (guards the write path: if the
    // serializer wrote the constant instead of game.rating_model_version, a caller that forgot to
    // stamp the version would still pass the version check above but be silently flagged stale).
    REQUIRE(!loaded->isRatingStale());
}

TEST_CASE("Loading a legacy save with no rating_model_version key defaults it to 0",
          "[integration][save_manager][rating_model]") {
    RatingModelCompatFixture fixture;

    auto loaded = fixture.loadRawYaml("golden_1_0", kGolden_1_0);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->rating_model_version == 0);
}

// ── AC4 + AC5(b): snapshot-preserve — stored literal survives, rating_stale flagged ──────

TEST_CASE("A save rated by an older model loads its STORED rating, not a recompute, and is flagged stale",
          "[integration][save_manager][rating_model][snapshot]") {
    RatingModelCompatFixture fixture;

    auto loaded = fixture.loadRawYaml("stale_rating", kStaleRatingSave);

    REQUIRE(loaded.has_value());
    // Snapshot-preserve: the deliberately-impossible stored value comes back verbatim.
    REQUIRE(loaded->puzzle_rating == 9.99);
    REQUIRE(loaded->difficulty == Difficulty::Master);
    REQUIRE(loaded->rating_model_version == 0);
    REQUIRE(loaded->isRatingStale());
}

TEST_CASE("Saving preserves a non-current rating_model_version (no promotion to the constant)",
          "[integration][save_manager][rating_model][snapshot][regression]") {
    // Regression for the firewall's core guarantee: the serializer must persist the game's OWN
    // rating_model_version, NOT the compile-time RATING_MODEL_VERSION. If it wrote the constant,
    // re-saving a legacy/stale game (auto-save, rename, checkpoint) would silently promote it to
    // the current version and permanently lose the stale marker without ever re-rating it.
    RatingModelCompatFixture fixture;

    SaveSettings settings;
    settings.compress = false;
    settings.encrypt = false;

    SavedGame stale = makeRatedGame("legacy_resave");
    stale.rating_model_version = 0;  // produced by an older model than this build
    REQUIRE(stale.isRatingStale());

    auto save_result = fixture.save_manager_.saveGame(stale, settings);
    REQUIRE(save_result.has_value());
    auto loaded = fixture.save_manager_.loadGame(*save_result);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->rating_model_version == 0);  // preserved, not promoted to RATING_MODEL_VERSION
    REQUIRE(loaded->isRatingStale());
}

TEST_CASE("A save rated by the current model is NOT flagged stale",
          "[integration][save_manager][rating_model][snapshot]") {
    RatingModelCompatFixture fixture;

    auto loaded = fixture.loadRawYaml("golden_1_1", kGolden_1_1);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->rating_model_version == RATING_MODEL_VERSION);
    // Use REQUIRE(!x) rather than REQUIRE_FALSE: the latter expands to the Catch2 disposition
    // flag-combo Normal|FalseTest (== 5), which trips clang-analyzer's EnumCastOutOfRange false
    // positive (it doesn't model bitflag enums) and fails the CI clang-tidy diff gate.
    REQUIRE(!loaded->isRatingStale());
}

// ── AC5(a): golden load-compat matrix — both versions load to known-good state ───────────

TEST_CASE("Golden 1.0 and 1.1 fixtures each load to a known-good state",
          "[integration][save_manager][rating_model][golden]") {
    RatingModelCompatFixture fixture;

    SECTION("1.0 golden save") {
        auto loaded = fixture.loadRawYaml("golden_1_0", kGolden_1_0);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->save_format_version == "1.0");
        REQUIRE(loaded->rating_model_version == 0);
        // The missing-key + zero-rating combination that characterises every pre-0b.0 legacy
        // save: it must read as stale (no model-version key → 0 ≠ current).
        REQUIRE(loaded->isRatingStale());
        REQUIRE(loaded->difficulty == Difficulty::Easy);
        REQUIRE(loaded->original_puzzle[0][0] == 5);
    }

    SECTION("1.1 golden save") {
        auto loaded = fixture.loadRawYaml("golden_1_1", kGolden_1_1);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->save_format_version == "1.1");
        REQUIRE(loaded->rating_model_version == 1);
        REQUIRE(!loaded->isRatingStale());
        REQUIRE(loaded->difficulty == Difficulty::Hard);
        REQUIRE(loaded->puzzle_rating == 4.5);
        REQUIRE(loaded->puzzle_technique_ids == std::vector<int>{0, 1, 11});
    }
}

// ── AC5(c): version write-assert — both keys emitted on save ─────────────────────────────

TEST_CASE("Saving emits both `version` and `rating_model_version` keys",
          "[integration][save_manager][rating_model][version]") {
    RatingModelCompatFixture fixture;

    SaveSettings settings;
    settings.compress = false;  // inspectable, plain-text YAML
    settings.encrypt = false;

    auto save_result = fixture.save_manager_.saveGame(makeRatedGame("write_assert"), settings);
    REQUIRE(save_result.has_value());

    auto save_path = fs::path(fixture.test_dir_) / (*save_result + ".yaml");
    std::ifstream in(save_path, std::ios::binary);
    REQUIRE(in.is_open());
    const std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    REQUIRE(contents.contains("version:"));
    REQUIRE(contents.contains("rating_model_version:"));
}

// ── AC5(d): unknown schema version → defined best-effort branch, never UB ────────────────

TEST_CASE("Loading a save with an unrecognized future schema version is a defined best-effort load",
          "[integration][save_manager][rating_model][version]") {
    RatingModelCompatFixture fixture;

    auto loaded = fixture.loadRawYaml("future_schema", kUnknownVersionSave);

    // Defined policy: best-effort load succeeds and preserves the unknown version verbatim.
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->save_format_version == "9.9");
}
