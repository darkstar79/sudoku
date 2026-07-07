#!/bin/bash

# Setup script for installing Git hooks
# Usage: ./scripts/setup-hooks.sh

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the root directory of the git repository
REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)

if [ -z "$REPO_ROOT" ]; then
    echo "❌ Error: Not in a git repository"
    exit 1
fi

echo -e "${YELLOW}Installing Git hooks for sudoku...${NC}\n"

# Install pre-commit hook
PRE_COMMIT_SRC="$REPO_ROOT/scripts/pre-commit"
PRE_COMMIT_DEST="$REPO_ROOT/.git/hooks/pre-commit"

if [ ! -f "$PRE_COMMIT_SRC" ]; then
    echo "❌ Error: Pre-commit hook template not found at $PRE_COMMIT_SRC"
    exit 1
fi

# Copy and make executable
cp "$PRE_COMMIT_SRC" "$PRE_COMMIT_DEST"
chmod +x "$PRE_COMMIT_DEST"

echo -e "${GREEN}✅ Pre-commit hook installed${NC}"
echo "   Location: .git/hooks/pre-commit"
echo ""
echo "The hook will run the following checks before each commit:"
echo "  • GPLv3 license header check      — always, ~instant"
echo "  • Code formatting (clang-format)   — always, ~instant"
echo "  • Static analysis on changed lines — opt-in: TIDY=1 git commit"
echo ""

# Install pre-push hook (clang-tidy CI-parity gate)
PRE_PUSH_SRC="$REPO_ROOT/scripts/pre-push"
PRE_PUSH_DEST="$REPO_ROOT/.git/hooks/pre-push"

if [ ! -f "$PRE_PUSH_SRC" ]; then
    echo "❌ Error: Pre-push hook template not found at $PRE_PUSH_SRC"
    exit 1
fi

cp "$PRE_PUSH_SRC" "$PRE_PUSH_DEST"
chmod +x "$PRE_PUSH_DEST"

echo -e "${GREEN}✅ Pre-push hook installed${NC}"
echo "   Location: .git/hooks/pre-push"
echo ""
echo "The hook mirrors the CI clang-tidy-diff job before each push:"
echo "  • clang-tidy on the branch diff vs main — blocks on warnings"
echo "  • Uses the CI-pinned clang-tidy version (warns on a local mismatch)"
echo "  • Skips when no C++ changed; bypass with: git push --no-verify"
echo ""
echo -e "${YELLOW}To skip hooks on commit:${NC}"
echo "  git commit --no-verify"
echo ""
echo -e "${YELLOW}To check a PR diff vs main:${NC}"
echo "  ./scripts/tidy-diff.sh main"
echo ""
echo -e "${GREEN}Setup complete!${NC}"
