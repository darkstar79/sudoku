#!/usr/bin/env bash
# check_release_consistency.sh — fails if the release version drifts across the three hand-maintained
# copies, or (in tag mode) if a pushed tag disagrees with the declared version or ships a stale
# metainfo release date. Backs the release/version-integrity gate (Code Review 2026-07-06, REL-3).
#
# The version is declared once in CMakeLists.txt (project(VERSION ...)) — the single source of truth —
# but mirrored by hand in two packager-visible files that CMake does not generate:
#   * conanfile.py                                    (version = "X.Y.Z")
#   * resources/linux/…​.metainfo.xml                  (first <release version="…" date="…">)
# Nothing else checks that these agree; this script is that check, used locally (CONTRIBUTING
# "Cutting a release") and in CI (the packaging.yml release-guard job).
#
# Modes:
#   (no args)          Consistency-only. Extract and pairwise-compare the three versions; fail on any
#                      mismatch or any failed extraction. Used on workflow_dispatch from a branch.
#   $1 = vX.Y.Z (tag)  Additionally assert  $1 == "v" + <CMake PROJECT_VERSION>  and that the metainfo
#                      top-release date falls within [tag_commit_date − 30 days, tag_commit_date].
#                      The reference date is the *tag commit* date (git log %cs), never the wall clock,
#                      so the verdict is deterministic and re-runnable. The 30-day window still catches
#                      a years-stale date while tolerating a release prepared/staged well before tagging.
#
# Portability caveat: unlike the general scripts policy, this one is GNU-tools-only — it relies on GNU
# `grep -P` (PCRE \K) and GNU `date -d`. It targets Linux/CI and Git Bash with GNU coreutils; stock
# macOS/BSD grep/date will not run it. That is acceptable: it runs in CI (ubuntu) and on the Linux dev
# machine, never as part of a user build.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CMAKE_FILE='CMakeLists.txt'
CONAN_FILE='conanfile.py'
METAINFO_FILE='resources/linux/io.github.darkstar79.Sudoku.metainfo.xml'

SEMVER='[0-9]+\.[0-9]+\.[0-9]+'

fail() {
  echo "❌ Release consistency gate: $1" >&2
  exit 1
}

# --- Extraction -------------------------------------------------------------------------------------

# CMake: scope to the project(...) block so cmake_minimum_required(VERSION 3.28) is never picked up.
cmake_version="$(sed -n '/^project(/,/^)/p' "$CMAKE_FILE" | grep -Pom1 "VERSION\s+\K${SEMVER}" || true)"
[ -n "$cmake_version" ] || fail "could not extract project(VERSION ...) from ${CMAKE_FILE}"

conan_version="$(grep -Pom1 "^\s*version\s*=\s*\"\K${SEMVER}" "$CONAN_FILE" || true)"
[ -n "$conan_version" ] || fail "could not extract version = \"X.Y.Z\" from ${CONAN_FILE}"

# Capture the FIRST <release ...> opening tag once, then pull BOTH attributes from that same element.
# Doing it in one pass (rather than two independent file-wide greps) keeps version and date bound to the
# same entry and makes the parse attribute-order-independent: a `date="…" version="…"` ordering, or a
# dateless top entry, can no longer read the two values from different <release> lines. `<release\b`
# won't match the `<releases>` container (no word boundary between "release" and "s").
release_line="$(grep -Pom1 '<release\b[^>]*>' "$METAINFO_FILE" || true)"
[ -n "$release_line" ] || fail "could not find a <release ...> element in ${METAINFO_FILE}"

metainfo_version="$(printf '%s' "$release_line" | grep -Pom1 "version=\"\K${SEMVER}" || true)"
[ -n "$metainfo_version" ] || fail "could not extract version=\"...\" from the top <release> in ${METAINFO_FILE}"

# Date may legitimately be absent in consistency-only mode; only tag mode (below) hard-requires it.
metainfo_date="$(printf '%s' "$release_line" | grep -Pom1 'date="\K[0-9-]+' || true)"

# --- Consistency (always) ---------------------------------------------------------------------------

if [ "$cmake_version" != "$conan_version" ]; then
  fail "version mismatch: ${CMAKE_FILE} declares ${cmake_version} but ${CONAN_FILE} declares ${conan_version}"
fi
if [ "$cmake_version" != "$metainfo_version" ]; then
  fail "version mismatch: ${CMAKE_FILE} declares ${cmake_version} but ${METAINFO_FILE} top <release> is ${metainfo_version}"
fi

echo "✅ Release consistency gate: version ${cmake_version} agrees across ${CMAKE_FILE}, ${CONAN_FILE}, and the metainfo."

# --- Tag mode (optional argument) -------------------------------------------------------------------

if [ "$#" -eq 0 ]; then
  exit 0
fi

tag="$1"
expected_tag="v${cmake_version}"
if [ "$tag" != "$expected_tag" ]; then
  fail "tag ${tag} does not match the declared version — expected ${expected_tag} (from project(VERSION ${cmake_version}))"
fi

tag_date="$(git log -1 --format=%cs "${tag}^{commit}" 2>/dev/null || true)"
[ -n "$tag_date" ] || fail "could not resolve commit date of tag ${tag} (does the tag exist locally?)"

# Tag mode needs a real date on the top <release>; refuse rather than silently reading a lower entry.
[ -n "$metainfo_date" ] || fail "the top <release> in ${METAINFO_FILE} has no date=\"...\" attribute — tag mode requires the release-day date on the newest entry."

# Deterministic window: [tag_date − 30 days, tag_date]. No wall clock — the reference is the tag commit.
tag_epoch="$(date -d "$tag_date" +%s)" || fail "could not parse tag commit date '${tag_date}'"
meta_epoch="$(date -d "$metainfo_date" +%s)" || fail "could not parse metainfo release date '${metainfo_date}' — expected YYYY-MM-DD."
lower_epoch="$(date -d "${tag_date} - 30 days" +%s)" || fail "could not compute the 30-day lower bound from tag commit date '${tag_date}'"

if [ "$meta_epoch" -gt "$tag_epoch" ] || [ "$meta_epoch" -lt "$lower_epoch" ]; then
  fail "metainfo release date ${metainfo_date} is outside the window [${tag_date} − 30 days, ${tag_date}] for tag ${tag}. Update the top <release date=\"...\"> in ${METAINFO_FILE} to the release day."
fi

echo "✅ Release tag gate: ${tag} matches project(VERSION) and metainfo date ${metainfo_date} is within 30 days of the tag commit (${tag_date})."
