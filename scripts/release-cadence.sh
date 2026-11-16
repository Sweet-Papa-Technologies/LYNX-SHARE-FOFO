#!/usr/bin/env bash
# release-cadence.sh nightly|weekly — what the Nightly beta and Sunday release workflows should tag, if anything.
#
# Picks the newest commit on main (first parent, last 20) whose required checks all passed, then:
#   nightly  tag v<version>-beta.<YYYYMMDD> when that commit is newer than the last v* tag
#   weekly   tag v<version> when there is anything since the last final release; next = the following patch version
# <version> is CMakeLists.txt's. When v<version> is already released, nothing is tagged until main is bumped.
#
# Writes tag=, sha= (and next= for weekly) to $GITHUB_OUTPUT, a summary to $GITHUB_STEP_SUMMARY. An empty tag means
# "nothing to do" and is not an error. Run locally (with gh signed in) to preview: scripts/release-cadence.sh nightly
set -euo pipefail
mode="${1:?usage: release-cadence.sh nightly|weekly}"
repo="${GITHUB_REPOSITORY:-Sweet-Papa-Technologies/LYNX-SHARE-FOFO}"
ignore_checks="${IGNORE_CHECKS:-false}"
required=("Server limits" "App (ubuntu-24.04)" "App (macos-15)")   # main's required status checks

out() { echo "$1=$2" >> "${GITHUB_OUTPUT:-/dev/stdout}"; }
say() { echo "$*"; if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then echo "$*" >> "$GITHUB_STEP_SUMMARY"; fi; }
nothing() { say "$*"; out tag ""; exit 0; }

version="$(sed -n 's/^project(LYNX VERSION \([0-9]*\.[0-9]*\.[0-9]*\).*/\1/p' CMakeLists.txt)"
[[ -n "$version" ]] || { echo "::error::no version in CMakeLists.txt"; exit 1; }

green() { # all required checks of commit $1 concluded success
  local runs name
  runs="$(gh api "repos/$repo/commits/$1/check-runs?per_page=100" -q '.check_runs[] | "\(.name)\t\(.conclusion)"')"
  for name in "${required[@]}"; do
    grep -q -x -F "$name"$'\t'"success" <<< "$runs" || return 1
  done
}

sha=""
if [[ "$ignore_checks" == true ]]; then
  sha="$(git rev-parse HEAD)"
  say "Checks ignored on request: using main's head ${sha::9}."
else
  for c in $(git rev-list --first-parent -n 20 HEAD); do
    if green "$c"; then sha="$c"; break; fi
  done
  [[ -n "$sha" ]] || nothing "No commit among main's last 20 has all required checks green (${required[*]}); nothing tagged."
  [[ "$sha" == "$(git rev-parse HEAD)" ]] || say "main's head is not green yet; using the newest green commit ${sha::9}."
fi
out sha "$sha"

git rev-parse -q --verify "refs/tags/v$version" > /dev/null \
  && nothing "v$version is already released: main needs the version bump (the Sunday release opens that pull request)."

last="$(git describe --tags --abbrev=0 --match 'v[0-9]*' "$sha" 2> /dev/null || true)"
if [[ -n "$last" && "$(git rev-list -n 1 "$last")" == "$sha" ]]; then
  nothing "Nothing new: ${sha::9} is already $last."
fi

case "$mode" in
  nightly)
    tag="v$version-beta.$(date -u +%Y%m%d)"
    git rev-parse -q --verify "refs/tags/$tag" > /dev/null && nothing "$tag already exists (one nightly a day)."
    if [[ "${PUBLISH:-false}" != true ]]; then
      say "Dry run (repository variable NIGHTLY_PUBLISH is not 'true'): would tag **$tag** at ${sha::9}."
    else
      say "Tagging **$tag** at ${sha::9} (since ${last:-the beginning})."
    fi
    ;;
  weekly)
    final="$(git tag -l 'v[0-9]*' --sort=-v:refname | grep -v -- - | head -n 1 || true)"
    if [[ -n "$final" ]] && [[ "$(git rev-list --count "$final..$sha")" == 0 ]]; then
      nothing "Nothing merged since $final; no release this week."
    fi
    tag="v$version"
    IFS=. read -r major minor patch <<< "$version"
    out next "$major.$minor.$((patch + 1))"
    say "## Proposed release: $tag at \`${sha::9}\`"
    say "Commits since ${final:-the beginning}: $(git rev-list --count ${final:+$final..}"$sha")."
    say "Approve the **release** job to tag and publish it; then open and merge the version bump pull request it links."
    git checkout -q "$sha"
    if [[ -x scripts/release-notes.sh ]]; then
      say ""
      say "### Release notes (preview)"
      say "$(scripts/release-notes.sh "$tag" 2> /dev/null || echo '(release-notes.sh failed)')"
    fi
    ;;
  *) echo "::error::unknown mode $mode"; exit 1 ;;
esac
out tag "$tag"
