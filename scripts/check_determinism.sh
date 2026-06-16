#!/usr/bin/env bash
# check_determinism.sh — fails if a wall-clock or unseeded-RNG bypass is introduced in src/ outside
# the single legal time bridge. Backs the determinism contract (GitHub #24): every non-deterministic
# time or RNG source in the engine must route through core::ITimeProvider (or a seeded std::mt19937)
# so the solver / generator / rater stay reproducible in tests and the AI-Escargot livelock class
# stays closed. Modeled on the i18n-check grep gate in .github/workflows/ci.yml.
#
# Detected bypass *calls* (narrowed from CODE_REVIEW_2026-05-25.md §3.2 to target the call, not the
# type, so member declarations like `system_clock::time_point t_;` are intentionally NOT flagged):
#   (steady_clock|system_clock)::now()   direct wall-clock reads
#   std::localtime                       non-reentrant (shared static buffer)
#   random_device                        unseeded entropy source
#
# Exemptions are explicit — never a blanket directory exclude (see #24 Task 7):
#   * src/core/i_time_provider.h          the one legal bridge (SystemTimeProvider/MockTimeProvider).
#   * a `// determinism-ok: <reason>` marker on the flagged line OR the line directly above it —
#     a documented, reviewed real-time exception (UI session timers, persisted wall-clock save
#     metadata, save-id entropy, internal timeout sampling). The above-line tolerance keeps the
#     marker attached even when clang-format wraps a long statement.
#
# Scope: src/ only. Tests legitimately read the real clock (benchmarks, temp-dir naming,
# MockTimeProvider construction) and are out of scope by design.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ALLOWED_FILE='src/core/i_time_provider.h'

fail=0
violations=""

while IFS= read -r file; do
  [ "$file" = "$ALLOWED_FILE" ] && continue
  # awk scans each file: flag a bypass call unless a determinism-ok marker is on that line or the
  # immediately preceding one. The regex matches the call form, not the type declaration.
  out="$(awk '
    /(steady_clock|system_clock)::now\(\)|std::localtime|random_device/ {
      if (index($0, "determinism-ok") == 0 && index(prev, "determinism-ok") == 0) {
        printf "%d:%s\n", NR, $0
      }
    }
    { prev = $0 }
  ' "$file")"
  if [ -n "$out" ]; then
    while IFS= read -r hit; do
      violations+="${file}:${hit}"$'\n'
    done <<< "$out"
    fail=1
  fi
done < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) | sort)

if [ "$fail" -eq 1 ]; then
  echo "❌ Determinism gate: wall-clock / unseeded-RNG bypass found in src/ (not routed through ITimeProvider):"
  echo ""
  printf '%s' "$violations"
  echo ""
  echo "Fix by routing through core::ITimeProvider (steadyNow()/systemNow()) or a seeded std::mt19937."
  echo "If the call is legitimately real-time (UI timer, persisted wall-clock, save-id entropy),"
  echo "annotate that line (or the line directly above) with '// determinism-ok: <reason>'."
  exit 1
fi

echo "✅ Determinism gate: no unguarded wall-clock / unseeded-RNG bypass in src/"
