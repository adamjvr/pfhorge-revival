#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

set -euo pipefail

apply=false
remote=false

usage() {
  cat <<'USAGE'
Usage: scripts/revival/cleanup_revival_branches.sh [--apply] [--remote]

Without --apply, prints branches that are safely merged into the current HEAD.
With --apply, deletes merged local revival/* branches using git branch -d.
With --remote as well, also deletes merged origin/revival/* branches.

Protected automatically:
  main, master, current branch, and any branch not fully merged into HEAD.
USAGE
}

while (($#)); do
  case "$1" in
    --apply) apply=true ;;
    --remote) remote=true ;;
    -h|--help) usage; exit 0 ;;
    *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

root="$(git rev-parse --show-toplevel)"
cd "$root"

current="$(git branch --show-current)"
if [[ -z "$current" ]]; then
  printf '%s\n' 'Refusing branch cleanup from detached HEAD.' >&2
  exit 1
fi

if ! git diff --quiet || ! git diff --cached --quiet; then
  printf '%s\n' 'Refusing branch cleanup with uncommitted changes.' >&2
  exit 1
fi

git fetch --prune origin

local_candidates="$(
  git for-each-ref \
    --format='%(refname:short)' \
    --merged HEAD \
    refs/heads/revival/ \
  | while IFS= read -r branch; do
      [[ "$branch" == "$current" ]] && continue
      printf '%s\n' "$branch"
    done
)"

printf 'Current integration head: %s\n' "$current"
printf '%s\n' 'Merged local revival branches:'
if [[ -z "$local_candidates" ]]; then
  printf '%s\n' '  (none)'
else
  while IFS= read -r branch; do
    [[ -n "$branch" ]] || continue
    printf '  %s\n' "$branch"
  done <<< "$local_candidates"
fi

if $apply; then
  while IFS= read -r branch; do
    [[ -n "$branch" ]] || continue
    git branch -d "$branch"
  done <<< "$local_candidates"
else
  printf '%s\n' 'Dry run only. Add --apply to delete the listed local branches.'
fi

if $remote; then
  remote_candidates="$(
    git for-each-ref --format='%(refname:short)' refs/remotes/origin/revival/ \
    | while IFS= read -r remote_ref; do
        branch="${remote_ref#origin/}"
        [[ "$branch" == "$current" ]] && continue
        if git merge-base --is-ancestor "$remote_ref" HEAD; then
          printf '%s\n' "$branch"
        fi
      done
  )"

  printf '%s\n' 'Merged remote revival branches:'
  if [[ -z "$remote_candidates" ]]; then
    printf '%s\n' '  (none)'
  else
    while IFS= read -r branch; do
      [[ -n "$branch" ]] || continue
      printf '  %s\n' "$branch"
    done <<< "$remote_candidates"
  fi

  if $apply; then
    while IFS= read -r branch; do
      [[ -n "$branch" ]] || continue
      git push origin --delete "$branch"
    done <<< "$remote_candidates"
  else
    printf '%s\n' 'Remote cleanup is also a dry run until --apply is supplied.'
  fi
fi
