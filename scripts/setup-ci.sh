#!/usr/bin/env bash
#
# setup-ci.sh — idempotent setup of the release signing and branch protection
# pipeline on GitHub for Sweet-Papa-Technologies/LYNX-SHARE-FOFO.
#
#   ./scripts/setup-ci.sh                 # everything
#   ./scripts/setup-ci.sh --branch        # protect main + configure merge settings
#   ./scripts/setup-ci.sh --apple         # Apple Developer ID cert + notarisation
#   ./scripts/setup-ci.sh --notary        # just the notarisation credential
#   ./scripts/setup-ci.sh --azure         # Azure Trusted Signing OIDC variables
#   ./scripts/setup-ci.sh --show          # what is configured right now
#
set -euo pipefail

REPO="${REPO:-Sweet-Papa-Technologies/LYNX-SHARE-FOFO}"
DO_BRANCH=0 DO_APPLE=0 DO_AZURE=0 DO_SHOW=0 DO_NOTARY=0
IDENTITY_WANT=""
CERT_P12_ARG=""
ENV_FILE="${ENV_FILE:-$(cd "$(dirname "$0")/.." && pwd)/.env}"

die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarn:\033[0m %s\n' "$*" >&2; }
ask()  { printf '\033[35m ? \033[0m%s ' "$*"; }

if [ $# -eq 0 ]; then DO_BRANCH=1; DO_APPLE=1; DO_AZURE=1; fi
while [ $# -gt 0 ]; do
    case "$1" in
        --branch) DO_BRANCH=1; shift ;;
        --apple)  DO_APPLE=1;  shift ;;
        --azure)  DO_AZURE=1;  shift ;;
        --notary) DO_NOTARY=1; shift ;;
        --identity) IDENTITY_WANT="${2:?--identity needs a value}"; shift 2 ;;
        --cert)     CERT_P12_ARG="${2:?--cert needs a path}"; shift 2 ;;
        --env)      ENV_FILE="${2:?--env needs a path}"; shift 2 ;;
        --show)   DO_SHOW=1;   shift ;;
        --repo)   REPO="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,12p' "$0" | sed 's/^#//;s/^ //'; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

command -v gh >/dev/null || die "the GitHub CLI is not installed — https://cli.github.com"
gh auth status >/dev/null 2>&1 || die "not logged in — run: gh auth login"

ASC_KEY_ID_DEFAULT="${ASC_KEY_ID:-45S6SMU5GY}"
ASC_ISSUER_DEFAULT="${ASC_ISSUER_ID:-08a7930e-9034-41cd-ab35-41afa2b19813}"
ASC_KEY_SERVICE="${ASC_KEY_SERVICE:-asc-mcp}"

read_asc_key() {
    local raw
    raw="$(security find-generic-password -s "$ASC_KEY_SERVICE" -a "$1" -w 2>/dev/null || true)"
    [ -n "$raw" ] || return 1
    case "$raw" in
        *BEGIN*) printf '%s\n' "$raw" ;;
        *)       printf '%s' "$raw" | xxd -r -p 2>/dev/null ;;
    esac
}

set_notary_secrets() {
    local key="" kid="$ASC_KEY_ID_DEFAULT"
    info "notarisation credential"
    if key="$(read_asc_key "$kid")" && printf '%s' "$key" | grep -q "BEGIN PRIVATE KEY"; then
        echo "    App Store Connect API key $kid (from the '$ASC_KEY_SERVICE' keychain item)"
    else
        local found
        found="$(ls "$HOME"/code/*/config/AuthKey_*.p8 "$HOME"/Downloads/AuthKey_*.p8 \
                 "$HOME"/.appstoreconnect/private_keys/AuthKey_*.p8 2>/dev/null | head -1 || true)"
        if [ -n "$found" ]; then
            kid="$(basename "$found" | sed 's/AuthKey_\(.*\)\.p8/\1/')"
            key="$(cat "$found")"
            echo "    App Store Connect API key $kid (from $found)"
        fi
    fi

    if printf '%s' "$key" | grep -q "BEGIN PRIVATE KEY"; then
        printf '%s' "$key" | base64 | gh secret set ASC_KEY_P8   -R "$REPO"
        printf '%s' "$kid"                | gh secret set ASC_KEY_ID    -R "$REPO"
        printf '%s' "$ASC_ISSUER_DEFAULT" | gh secret set ASC_ISSUER_ID -R "$REPO"
        echo "    3 secrets set (ASC_KEY_P8, ASC_KEY_ID, ASC_ISSUER_ID)"
        return 0
    fi
    warn "no App Store Connect API key found"
    return 1
}

if [ "$DO_SHOW" = 1 ]; then
    info "secrets on $REPO"
    gh secret list -R "$REPO" 2>/dev/null | sed 's/^/    /' || echo "    (none)"
    info "variables on $REPO"
    gh variable list -R "$REPO" 2>/dev/null | sed 's/^/    /' || echo "    (none)"
    info "branches"
    gh api "repos/$REPO/branches" --jq '.[] | "    \(.name)\(if .protected then "  [protected]" else "" end)"'
    exit 0
fi

if [ "$DO_BRANCH" = 1 ]; then
    info "repository merge settings on $REPO"
    gh api -X PATCH "repos/$REPO" \
        -F allow_squash_merge=true \
        -F allow_merge_commit=true \
        -F allow_rebase_merge=true \
        -F delete_branch_on_merge=true \
        -F allow_auto_merge=true >/dev/null

    info "branch protection on main"
    gh api -X PUT "repos/$REPO/branches/main/protection" \
        --input - >/dev/null <<'JSON'
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "Server limits",
      "App (ubuntu-24.04)",
      "App (macos-15)"
    ]
  },
  "enforce_admins": false,
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": false,
    "required_approving_review_count": 1
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "required_linear_history": false,
  "required_conversation_resolution": true
}
JSON
    echo "    protected main: PR + 1 review (admins bypassable), CI checks must pass, no force push, no deletion"
fi

if [ "$DO_APPLE" = 1 ]; then
    info "Apple signing certificate"
    P12=""; P12PASS=""
    if [ -f "$ENV_FILE" ]; then
        # shellcheck disable=SC1090
        set -a; . "$ENV_FILE"; set +a
        if [ -n "${CERT_LOCATION:-}" ]; then
            P12="${CERT_LOCATION/#\~/$HOME}"
            P12PASS="${CERT_PASS:-}"
            echo "    using the .p12 named in $ENV_FILE"
        fi
    fi
    if [ -z "$P12" ] && [ -n "$CERT_P12_ARG" ]; then
        P12="${CERT_P12_ARG/#\~/$HOME}"
    fi
    [ -f "$P12" ] || die "no .p12 at $P12 (pass --env /path/to/.env or --cert /path/to.p12)"
    if [ -z "$P12PASS" ]; then
        ask "the password the .p12 was exported with:"
        read -rs P12PASS; echo
    fi
    PROBE="$HOME/.setup-ci-probe.keychain-db"
    PROBE_PASS=$(openssl rand -hex 16)
    security delete-keychain "$PROBE" 2>/dev/null || true
    security create-keychain -p "$PROBE_PASS" "$PROBE"
    security unlock-keychain -p "$PROBE_PASS" "$PROBE"
    if security import "$P12" -k "$PROBE" -P "$P12PASS" -T /usr/bin/codesign >/dev/null 2>&1; then
        FOUND=$(security find-identity -v -p codesigning "$PROBE" \
                | grep -F "Developer ID Application" | head -1 || true)
        NAME=$(printf '%s' "$FOUND" | sed 's/.*"\(.*\)".*/\1/')
        TEAM=$(printf '%s' "$NAME" | sed -n 's/.*(\([A-Z0-9]\{10\}\))$/\1/p')
        security delete-keychain "$PROBE" 2>/dev/null || true
        [ -n "$NAME" ] || die "the .p12 imported but holds no Developer ID Application identity"
        echo "    contains: $NAME"
        echo "    team:     $TEAM"
    else
        security delete-keychain "$PROBE" 2>/dev/null || true
        die "could not import the .p12 — wrong password, or it has no private key"
    fi
    base64 -i "$P12" | gh secret set APPLE_CERT_P12            -R "$REPO"
    printf '%s' "$P12PASS" | gh secret set APPLE_CERT_PASSWORD  -R "$REPO"
    printf '%s' "$TEAM"    | gh secret set APPLE_TEAM_ID        -R "$REPO"
    echo "    3 secrets set (APPLE_CERT_P12, APPLE_CERT_PASSWORD, APPLE_TEAM_ID)"
    set_notary_secrets
fi

if [ "$DO_NOTARY" = 1 ]; then set_notary_secrets; fi

if [ "$DO_AZURE" = 1 ]; then
    info "Azure Trusted Signing (OIDC)"
    command -v az >/dev/null || { warn "no Azure CLI; skipping"; exit 0; }
    ACC=$(az account show -o json 2>/dev/null || true)
    [ -n "$ACC" ] || { warn "not logged in to Azure (az login); skipping"; exit 0; }
    TENANT=$(printf '%s' "$ACC" | sed -n 's/.*"tenantId": *"\([^"]*\)".*/\1/p' | head -1)
    SUB=$(printf '%s' "$ACC" | sed -n 's/.*"id": *"\([^"]*\)".*/\1/p' | head -1)
    CLIENTID="${AZURE_CLIENT_ID:-6ffedacb-25e4-49fc-9ccc-3379819fe8b3}"
    gh variable set AZURE_CLIENT_ID       -R "$REPO" -b "$CLIENTID"
    gh variable set AZURE_TENANT_ID       -R "$REPO" -b "$TENANT"
    gh variable set AZURE_SUBSCRIPTION_ID -R "$REPO" -b "$SUB"
    echo "    3 variables set (AZURE_CLIENT_ID, AZURE_TENANT_ID, AZURE_SUBSCRIPTION_ID)"
fi

info "done"
