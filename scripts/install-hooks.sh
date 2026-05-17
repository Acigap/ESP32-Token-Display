#!/usr/bin/env bash
# scripts/install-hooks.sh
#
# Copies the project's git hooks into .git/hooks and makes them executable.
# Run once after cloning:  bash scripts/install-hooks.sh
# ─────────────────────────────────────────────────────────

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
HOOKS_SRC="${REPO_ROOT}/scripts"
HOOKS_DST="${REPO_ROOT}/.git/hooks"

install_hook() {
    local name="$1"
    if [[ -f "${HOOKS_SRC}/${name}" ]]; then
        cp "${HOOKS_SRC}/${name}" "${HOOKS_DST}/${name}"
        chmod +x "${HOOKS_DST}/${name}"
        echo "  Installed: ${name}"
    fi
}

echo "Installing git hooks..."
install_hook pre-commit
echo "Done. Hooks are active for this repository."
