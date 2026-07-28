#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CREATE_BRANCH=1
RUN_BUILD=1
REQUIRE_CLEAN=0
BRANCH="revival/stage-1-baseline"

usage() {
  cat <<'USAGE'
Usage: scripts/revival/bootstrap_macos.sh [options]

Options:
  --no-branch       Do not create/switch to the revival branch.
  --audit-only      Run the source audit without compiling.
  --require-clean   Refuse to proceed if the working tree has changes.
  --branch NAME     Branch name (default: revival/stage-1-baseline).
  -h, --help        Show help.
USAGE
}

while (($#)); do
  case "$1" in
    --no-branch) CREATE_BRANCH=0 ;;
    --audit-only) RUN_BUILD=0 ;;
    --require-clean) REQUIRE_CLEAN=1 ;;
    --branch)
      shift
      BRANCH="${1:?missing branch name}"
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

cd "$ROOT"

if [[ ! -d .git ]]; then
  echo "error: run this inside a Git checkout of Pfhorge" >&2
  exit 2
fi

if [[ -n "$(git status --porcelain)" ]]; then
  if ((REQUIRE_CLEAN)); then
    echo "error: working tree is not clean; commit or stash changes first" >&2
    exit 2
  fi
  echo "warning: carrying current uncommitted changes onto the revival branch" >&2
fi

if ((CREATE_BRANCH)); then
  if git show-ref --verify --quiet "refs/heads/$BRANCH"; then
    git switch "$BRANCH"
  else
    git switch -c "$BRANCH"
  fi
fi

python3 scripts/revival/source_audit.py --root . --output-dir RevivalArtifacts

if ((RUN_BUILD)); then
  scripts/revival/baseline_build.sh
fi
