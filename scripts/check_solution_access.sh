#!/usr/bin/env bash
# check_solution_access.sh — fails if a src/ caller reads GameState::getSolutionBoard() without a
# hasSolution() gate. Backs story 8.16 / AC6.
#
# Why this gate exists: solution_board_ is a std::optional that is EMPTY for two entirely normal
# game shapes — a game restored from a save (the save format has no solution field) and a
# custom-puzzle edit-mode game (commitEditedPuzzle never runs the solver). getSolutionBoard()
# dereferences it, so an ungated read throws std::bad_optional_access. In story 8.16 that read sat
# in resetGame(), the throw escaped a Qt slot on the default launch path (play → quit → relaunch
# resumes the auto-save → press Reset), and the process died before the exit auto-save could run.
#
# The precondition used to live only in a `// callers gate on hasSolution()` comment carrying a
# bugprone-unchecked-optional-access NOLINT — a contract that one of its three callers silently
# broke and that static analysis was therefore suppressed from finding. GameState now also offers
# trySolutionBoard() (returns std::optional), which is the right accessor wherever the gate cannot
# be established locally; this script keeps the reference overload honest for the rest.
#
# Rule: a getSolutionBoard() call must have a hasSolution() check within the preceding
# kLookback lines of the same file. That window covers the guarded-block idiom the codebase uses:
#
#     if (state.hasSolution()) {
#         const auto& solution = state.getSolutionBoard();
#
# Scope: src/ only. Tests deliberately call getSolutionBoard() straight after a REQUIRE(hasSolution())
# or on a known-generated fixture, and a failing test is a visible failure rather than a shipped
# crash — they are out of scope by design, matching check_determinism.sh.
#
# Escape hatch: a `// solution-access-ok: <reason>` marker on the flagged line or the line directly
# above it, for a call whose gate genuinely lives further away (document why).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# The declaration itself and its own doc comments live here.
ALLOWED_FILE='src/model/game_state.h'

# Lines of lookback for the gate. Deliberately small: a gate further than this from its use is not
# reviewable at a glance, which is the failure mode this gate exists to prevent.
LOOKBACK=5

fail=0
violations=""

while IFS= read -r file; do
  [ "$file" = "$ALLOWED_FILE" ] && continue
  # Comments are stripped before anything is matched. Without that, a line comment merely NAMING
  # hasSolution() silences the gate — and the comment "callers gate on hasSolution()" is the exact
  # thing whose removal this gate exists to enforce, so the gate would have been blind to the very
  # defect it was written for. A commented-out call is likewise not a call and must not be flagged.
  # The `solution-access-ok` marker is matched on the RAW line, since it lives in a comment.
  out="$(awk -v lookback="$LOOKBACK" '
    {
      raw = $0
      code = $0
      sub(/\/\/.*/, "", code)
      if (code ~ /getSolutionBoard[[:space:]]*\(/) {
        gated = 0
        if (index(raw, "solution-access-ok") != 0 || index(prev_raw[1], "solution-access-ok") != 0) {
          gated = 1
        }
        if (code ~ /hasSolution[[:space:]]*\(/) {
          gated = 1
        }
        for (i = 1; i <= lookback && gated == 0; i++) {
          if (prev[i] ~ /hasSolution[[:space:]]*\(/) {
            gated = 1
          }
        }
        if (gated == 0) {
          printf "%d:%s\n", NR, raw
        }
      }
      for (i = lookback; i > 1; i--) {
        prev[i] = prev[i - 1]
        prev_raw[i] = prev_raw[i - 1]
      }
      prev[1] = code
      prev_raw[1] = raw
    }
  ' "$file")"
  if [ -n "$out" ]; then
    while IFS= read -r hit; do
      violations+="${file}:${hit}"$'\n'
    done <<< "$out"
    fail=1
  fi
done < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) | sort)

if [ "$fail" -eq 1 ]; then
  echo "❌ Solution-access gate: ungated GameState::getSolutionBoard() read in src/:"
  echo ""
  printf '%s' "$violations"
  echo ""
  echo "getSolutionBoard() throws on a restored or edit-mode game, which carries no solution board."
  echo "Either wrap the read in 'if (state.hasSolution())', or use trySolutionBoard() and handle the"
  echo "empty case. If the gate legitimately lives further away, annotate that line (or the line"
  echo "directly above) with '// solution-access-ok: <reason>'."
  exit 1
fi

echo "✅ Solution-access gate: every src/ getSolutionBoard() read is behind a hasSolution() check"
