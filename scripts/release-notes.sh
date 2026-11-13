#!/usr/bin/env bash
# Release notes for a GitHub release, as Markdown on stdout: what is new, what was fixed and what else changed since
# the previous release. The release workflow (.github/workflows/release.yml) uses it as the release's text, and the
# app's Update window shows that same text under "What's new" (src/core/UpdateCheck.cpp reads headings and bullets).
#
#   scripts/release-notes.sh <tag> [<previous tag>]
#       <tag>           the release's tag (v0.1.27). When the tag does not exist yet (a manual workflow run, or a
#                       preview before tagging) the notes cover HEAD.
#       <previous tag>  where the list starts; default: the nearest earlier release tag. A final release (v0.1.27)
#                       starts at the last final release, so pre-release tags in between do not cut its list short.
#
# The notes are made of:
#   1. docs/release-notes/<version>.md, when that file exists: hand-written highlights, put first as they are;
#   2. one line per change on the release branch since the previous tag (a merged pull request, or a commit pushed
#      directly), sorted into New / Fixed / Other changes by how its title starts ("feat:", "Add ..." -> New;
#      "fix:", "Fix ..." -> Fixed). Version bumps are left out. So a pull request's title is its release note:
#      write it for the person reading "What's new";
#   3. the build's provenance (the workflow passes it in RELEASE_NOTES_FOOTER) and a link to the full comparison.
#
# Needs the history back to the previous tag (CI: actions/checkout with fetch-depth: 0).
set -euo pipefail

tag="${1:?usage: release-notes.sh <tag> [<previous tag>]}"
cd "$(git rev-parse --show-toplevel)"

if git rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
  ref="$tag"
  before="$tag^" # (the tag itself is not its own previous release)
else
  ref=HEAD
  before=HEAD
fi

previous="${2:-}"
if [[ -z "$previous" ]]; then
  version="${tag#v}"
  if [[ "$version" == *-* ]]; then
    previous="$(git describe --tags --abbrev=0 --match 'v[0-9]*' "$before" 2> /dev/null || true)"
  else
    previous="$(git describe --tags --abbrev=0 --match 'v[0-9]*' --exclude '*-*' "$before" 2> /dev/null || true)"
  fi
fi
range="$ref"
[[ -n "$previous" ]] && range="$previous..$ref"

new=() fixed=() other=()

# "fix(ui): the thing (#12)" -> "The thing (#12)"
clean() {
  local s="$1"
  s="$(printf '%s' "$s" | sed -E 's/^[a-z]+(\([^)]*\))?!?:[[:space:]]*//')"
  printf '%s%s' "$(printf '%s' "${s:0:1}" | tr '[:lower:]' '[:upper:]')" "${s:1}"
}

while IFS= read -r commit; do
  [[ -n "$commit" ]] || continue
  subject="$(git log -1 --format=%s "$commit")"
  # a pull request merged with a merge commit: its title is the first line of the body
  if [[ "$subject" =~ ^Merge\ pull\ request\ #([0-9]+) ]]; then
    number="${BASH_REMATCH[1]}"
    title="$(git log -1 --format=%b "$commit" | sed -n '/[^[:space:]]/{p;q;}')"
    subject="${title:-$subject} (#$number)"
  fi
  lower="$(printf '%s' "$subject" | tr '[:upper:]' '[:lower:]')"
  case "$lower" in
    "bump version"* | "chore: bump version"* | "chore(release)"* | "release "[0-9]*) continue ;;
    feat:* | feat\(* | feat!:* | "add "* | "implement "*) new+=("$(clean "$subject")") ;;
    fix:* | fix\(* | fix!:* | "fix "* | "fixes "* | "fixed "*) fixed+=("$(clean "$subject")") ;;
    *) other+=("$(clean "$subject")") ;;
  esac
done < <(git rev-list --first-parent "$range")

highlights="docs/release-notes/${tag#v}.md"
if [[ -f "$highlights" ]]; then
  cat "$highlights"
  echo
fi

section() {
  local title="$1"
  shift
  [[ $# -gt 0 ]] || return 0
  echo "## $title"
  printf -- '- %s\n' "$@"
  echo
}
section "New" ${new[@]+"${new[@]}"}
section "Fixed" ${fixed[@]+"${fixed[@]}"}
section "Other changes" ${other[@]+"${other[@]}"}

if [[ ${#new[@]} -eq 0 && ${#fixed[@]} -eq 0 && ${#other[@]} -eq 0 && ! -f "$highlights" ]]; then
  echo "No changes since ${previous:-the start of the history}."
  echo
fi

[[ -n "${RELEASE_NOTES_FOOTER:-}" ]] && printf '%s\n\n' "$RELEASE_NOTES_FOOTER"
if [[ -n "$previous" && -n "${GITHUB_REPOSITORY:-}" ]]; then
  echo "**Full changelog**: ${GITHUB_SERVER_URL:-https://github.com}/$GITHUB_REPOSITORY/compare/$previous...$tag"
fi
