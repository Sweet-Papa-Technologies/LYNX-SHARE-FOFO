#!/usr/bin/env bash
#
# sign-windows.sh — Authenticode-sign Windows binaries (.exe / .dll) with Azure
# Trusted Signing, using whatever account `az login` is already holding.
#
#   ./scripts/sign-windows.sh bin/LYNX.exe bin/LYNX-bridge32.exe bin/lynx-asio.dll
#   ./scripts/sign-windows.sh dist/LYNX-*-setup-x64.exe --description "LYNX SHARE Installer"
#   ./scripts/sign-windows.sh bin/LYNX.exe --dry-run
#
# Trusted Signing itself is a web service, and --verify-setup works from any
# platform with the Azure CLI. The SIGNING step must run on Windows because the
# Microsoft client (dotnet/sign) P/Invokes kernel32.dll on startup.
#
set -euo pipefail

# ---- defaults, discovered from the Sweet Papa Technologies account ----
ENDPOINT="${TRUSTED_SIGNING_ENDPOINT:-https://eus.codesigning.azure.net/}"
ACCOUNT="${TRUSTED_SIGNING_ACCOUNT:-spt-cert}"
PROFILE="${TRUSTED_SIGNING_PROFILE:-forrester-personal}"
RESOURCE_GROUP="${TRUSTED_SIGNING_RG:-spt}"

DESCRIPTION="${SIGN_DESCRIPTION:-}"
DESCRIPTION_URL="${SIGN_DESCRIPTION_URL:-https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO}"
TIMESTAMP_URL="${SIGN_TIMESTAMP_URL:-http://timestamp.acs.microsoft.com}"
DRY_RUN=0
VERIFY_ONLY=0
TARGETS=()

die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarn:\033[0m %s\n' "$*" >&2; }

usage() {
    cat <<'EOF'
sign-windows.sh — Authenticode signing via Azure Trusted Signing

Usage:
  ./scripts/sign-windows.sh <file.exe|file.dll> [more...] [options]

Options:
  --account <name>       Trusted Signing account      (default: spt-cert)
  --profile <name>       certificate profile          (default: forrester-personal)
  --endpoint <url>       regional endpoint            (default: https://eus.codesigning.azure.net/)
  --resource-group <rg>  only used by --verify-setup  (default: spt)
  --description <text>   signature description shown by Windows
  --verify-setup         check az login, the account and the profile, then stop
  --dry-run              resolve everything and print the command, do not sign
  -h, --help             this text

Environment overrides:
  TRUSTED_SIGNING_ENDPOINT  TRUSTED_SIGNING_ACCOUNT  TRUSTED_SIGNING_PROFILE
  TRUSTED_SIGNING_RG        SIGN_DESCRIPTION         SIGN_DESCRIPTION_URL
  SIGN_TIMESTAMP_URL

First-time setup on a new machine:
  az login
  dotnet tool install --global sign --prerelease
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --account)        ACCOUNT="${2:?}"; shift 2 ;;
        --profile)        PROFILE="${2:?}"; shift 2 ;;
        --endpoint)       ENDPOINT="${2:?}"; shift 2 ;;
        --resource-group) RESOURCE_GROUP="${2:?}"; shift 2 ;;
        --description)    DESCRIPTION="${2:?}"; shift 2 ;;
        --verify-setup)   VERIFY_ONLY=1; shift ;;
        --dry-run)        DRY_RUN=1; shift ;;
        -h|--help)        usage; exit 0 ;;
        -*)               die "unknown option: $1" ;;
        *)                TARGETS+=("$1"); shift ;;
    esac
done

# ---- azure login -----------------------------------------------------
command -v az >/dev/null || die "the Azure CLI is not installed — https://aka.ms/azcli"

ACCOUNT_JSON="$(az account show -o json 2>/dev/null || true)"
[ -n "$ACCOUNT_JSON" ] || die "not logged in to Azure — run: az login"
AZ_USER="$(printf '%s' "$ACCOUNT_JSON" | sed -n 's/.*"name": *"\([^"]*\)".*/\1/p' | head -1)"
info "azure      ${AZ_USER:-authenticated}"
info "account    $ACCOUNT / $PROFILE"
info "endpoint   $ENDPOINT"

if [ "$VERIFY_ONLY" = 1 ]; then
    info "checking the signing account exists and the profile is active"
    az trustedsigning certificate-profile show \
        --account-name "$ACCOUNT" -g "$RESOURCE_GROUP" -n "$PROFILE" \
        --query "{profile:name, status:status, type:profileType}" -o table 2>&1 \
        | grep -v -i '^WARNING' | sed 's/^/           /'
    info "setup looks good"
    exit 0
fi

[ ${#TARGETS[@]} -gt 0 ] || { usage; exit 1; }
for f in "${TARGETS[@]}"; do [ -f "$f" ] || die "no such file: $f"; done

# ---- platform gate ---------------------------------------------------
case "$(uname -s)" in
    CYGWIN*|MINGW*|MSYS*|Windows_NT) IS_WINDOWS=1 ;;
    *)                               IS_WINDOWS=0 ;;
esac
if [ "$IS_WINDOWS" = 0 ] && [ "$DRY_RUN" = 0 ]; then
    die "Authenticode signing has to run on Windows.
  The Microsoft client (dotnet/sign) P/Invokes kernel32.dll on startup, so it
  aborts on $(uname -s) even though it ships as a portable .NET tool.
  --verify-setup and --dry-run work here; the release workflow signs on a
  windows-latest runner."
fi

# ---- the signing tool ------------------------------------------------
SIGN_BIN=""
if command -v sign >/dev/null 2>&1; then
    SIGN_BIN="sign"
elif [ -x "$HOME/.dotnet/tools/sign" ]; then
    SIGN_BIN="$HOME/.dotnet/tools/sign"
fi

if [ -z "$SIGN_BIN" ]; then
    if [ "$DRY_RUN" = 1 ]; then
        warn "the 'sign' tool is not installed; --dry-run will print the command anyway"
        SIGN_BIN="sign"
    else
        command -v dotnet >/dev/null || die "neither 'sign' nor 'dotnet' is installed.
  install the .NET SDK (https://dot.net) then:
    dotnet tool install --global sign --prerelease"
        if ! dotnet --list-sdks 2>/dev/null | grep -q .; then
            die "a .NET runtime is present but no SDK, so 'dotnet tool install' cannot run.
  install the .NET SDK from https://dot.net, then:
    dotnet tool install --global sign --prerelease"
        fi
        info "installing the 'sign' tool (one time)"
        dotnet tool install --global sign --prerelease
        SIGN_BIN="${HOME}/.dotnet/tools/sign"
        command -v sign >/dev/null 2>&1 && SIGN_BIN="sign"
    fi
fi

# ---- sign ------------------------------------------------------------
for f in "${TARGETS[@]}"; do
    DESC="${DESCRIPTION:-$(basename "${f%.*}")}"
    DIR="$(cd "$(dirname "$f")" && pwd)"
    FILE="$(basename "$f")"
    command -v cygpath >/dev/null 2>&1 && DIR="$(cygpath -w "$DIR")"

    set -- code trusted-signing "$FILE" \
        --base-directory "$DIR" \
        --trusted-signing-endpoint "$ENDPOINT" \
        --trusted-signing-account "$ACCOUNT" \
        --trusted-signing-certificate-profile "$PROFILE" \
        --description "$DESC" \
        --description-url "$DESCRIPTION_URL" \
        --timestamp-url "$TIMESTAMP_URL" \
        --file-digest SHA256 \
        --verbosity Information

    if [ "$DRY_RUN" = 1 ]; then
        info "dry run — would sign $FILE with:"
        printf '           %s' "$SIGN_BIN"
        for a in "$@"; do printf ' %q' "$a"; done
        printf '\n'
        continue
    fi

    info "signing    $FILE"
    "$SIGN_BIN" "$@" 2>&1 | sed 's/^/           /'
    info "signed     $f"
done

[ "$DRY_RUN" = 1 ] && exit 0

# ---- verify ----------------------------------------------------------
if command -v osslsigncode >/dev/null 2>&1; then
    for f in "${TARGETS[@]}"; do
        info "verifying  $(basename "$f")"
        osslsigncode verify "$f" 2>&1 | grep -E "Signature|Timestamp|Subject|Result" \
            | sed 's/^/           /' || true
    done
elif command -v signtool >/dev/null 2>&1; then
    for f in "${TARGETS[@]}"; do
        signtool verify /pa /v "$f" 2>&1 | sed 's/^/           /' || true
    done
else
    warn "no verifier found (osslsigncode or signtool) — signature not independently checked"
fi

info "done"
