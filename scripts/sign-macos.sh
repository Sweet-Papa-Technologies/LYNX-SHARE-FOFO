#!/usr/bin/env bash
#
# sign-macos.sh — sign, notarize and staple a macOS .app or .dmg build.
#
#   ./scripts/sign-macos.sh build/LYNX_artefacts/Release/LYNX.app
#   ./scripts/sign-macos.sh dist/LYNX-0.1.24-macos-universal.dmg
#   ./scripts/sign-macos.sh build/LYNX_artefacts/Release/LYNX.app --dry-run
#
set -euo pipefail

PRESET_SPT=( "spt-notary" "fterry24v2@gmail.com" "6Y5SZ2K5XY" "Developer ID Application: Forrester Terry" )

IDENTITY_PRESET="spt"
ENTITLEMENTS="${ENTITLEMENTS:-$(cd "$(dirname "$0")/.." && pwd)/resources/LYNX.entitlements}"
MAKE_DMG=0
DRY_RUN=0
TARGET=""

OVERRIDE_CERT=""
OVERRIDE_APPLE_ID="${APPLE_ID:-}"
OVERRIDE_TEAM_ID="${APPLE_TEAM_ID:-}"
OVERRIDE_KEYCHAIN_SERVICE="${NOTARY_KEYCHAIN_SERVICE:-}"

ASC_KEY_ID="${ASC_KEY_ID:-45S6SMU5GY}"
ASC_ISSUER_ID="${ASC_ISSUER_ID:-08a7930e-9034-41cd-ab35-41afa2b19813}"
ASC_KEY_SERVICE="${ASC_KEY_SERVICE:-asc-mcp}"
ASC_KEY_PATH="${ASC_KEY_PATH:-}"
ASC_KEY_B64="${ASC_KEY_B64:-}"
NO_ASC=0

die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarn:\033[0m %s\n' "$*" >&2; }

usage() {
    cat <<'EOF'
sign-macos.sh — sign, notarize and staple a macOS .app or .dmg

Usage:
  ./scripts/sign-macos.sh <LYNX.app|file.dmg> [options]

Options:
  --identity <spt|NAME>          signing account or Developer ID substring (default: spt)
  --entitlements <path>          entitlements plist for .app signing (default: resources/LYNX.entitlements)
  --dmg                          also build and notarize a .dmg from a .app
  --dry-run                      sign and verify locally, do not submit to Apple
  --no-asc-key                   ignore the API key, use Apple ID + app password
  -h, --help                     this text
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --identity)     IDENTITY_PRESET="${2:?}"; shift 2 ;;
        --entitlements) ENTITLEMENTS="${2:?}"; shift 2 ;;
        --dmg)          MAKE_DMG=1; shift ;;
        --dry-run)      DRY_RUN=1; shift ;;
        --no-asc-key)   NO_ASC=1; shift ;;
        -h|--help)      usage; exit 0 ;;
        -*)             die "unknown option: $1" ;;
        *)              [ -z "$TARGET" ] || die "only one target, got '$TARGET' and '$1'"
                        TARGET="$1"; shift ;;
    esac
done

[ -n "$TARGET" ] || { usage; exit 1; }
[ -e "$TARGET" ] || die "no such file: $TARGET"
[ "$(uname -s)" = "Darwin" ] || die "this script only runs on macOS"
command -v codesign >/dev/null || die "codesign not found"
command -v xcrun    >/dev/null || die "xcrun not found"

case "$IDENTITY_PRESET" in
    spt) P=( "${PRESET_SPT[@]}" ) ;;
    *)   P=( "" "" "" "$IDENTITY_PRESET" ) ;;
esac
KEYCHAIN_SERVICE="${OVERRIDE_KEYCHAIN_SERVICE:-${P[0]}}"
APPLE_ID_USED="${OVERRIDE_APPLE_ID:-${P[1]}}"
TEAM_ID="${OVERRIDE_TEAM_ID:-${P[2]}}"
CERT_MATCH="${SIGN_IDENTITY:-${OVERRIDE_CERT:-${P[3]}}}"

CERT_LINE="$(security find-identity -v -p codesigning 2>/dev/null | grep -F "$CERT_MATCH" | head -1 || true)"
[ -n "$CERT_LINE" ] || die "no codesigning identity matching '$CERT_MATCH'
  available identities:
$(security find-identity -v -p codesigning | sed 's/^/  /')"
CERT_HASH="$(printf '%s' "$CERT_LINE" | awk '{print $2}')"
CERT_NAME="$(printf '%s' "$CERT_LINE" | sed 's/.*"\(.*\)".*/\1/')"

if [ -z "$TEAM_ID" ]; then
    TEAM_ID="$(printf '%s' "$CERT_NAME" | sed -n 's/.*(\([A-Z0-9]\{10\}\))$/\1/p')"
fi

info "identity   $CERT_NAME"
info "team       ${TEAM_ID:-<unknown>}"

TARGET="${TARGET%/}"
BASENAME="$(basename "$TARGET")"
WORKDIR="$(cd "$(dirname "$TARGET")" && pwd)"
SIGN_TARGET="$WORKDIR/$BASENAME"

# ---- sign ------------------------------------------------------------
info "signing    $(basename "$SIGN_TARGET")"
case "$SIGN_TARGET" in
    *.app)
        if [ -d "$SIGN_TARGET/Contents/Frameworks" ]; then
            while IFS= read -r -d '' dylib; do
                info "signing    nested $(basename "$dylib")"
                codesign --force --options runtime --timestamp --sign "$CERT_HASH" "$dylib"
            done < <(find "$SIGN_TARGET/Contents/Frameworks" -type f \( -name "*.dylib" -o -name "*.so" \) -print0)
        fi
        ENT_ARGS=()
        if [ -f "$ENTITLEMENTS" ]; then
            ENT_ARGS=( --entitlements "$ENTITLEMENTS" )
        fi
        codesign --force --deep --options runtime --timestamp "${ENT_ARGS[@]}" --sign "$CERT_HASH" "$SIGN_TARGET"
        codesign --verify --deep --strict --verbose=2 "$SIGN_TARGET" 2>&1 | sed 's/^/           /'
        ;;
    *.dmg|*.pkg)
        codesign --force --timestamp --sign "$CERT_HASH" "$SIGN_TARGET"
        codesign --verify --verbose=2 "$SIGN_TARGET" 2>&1 | sed 's/^/           /'
        ;;
    *)
        codesign --force --options runtime --timestamp --sign "$CERT_HASH" "$SIGN_TARGET"
        codesign --verify --strict --verbose=2 "$SIGN_TARGET" 2>&1 | sed 's/^/           /'
        ;;
esac

if [ "$DRY_RUN" = 1 ]; then
    info "dry run — signed and verified, nothing submitted to Apple"
    exit 0
fi

# ---- notarize --------------------------------------------------------
[ -n "$TEAM_ID" ] || die "could not determine the team id — pass APPLE_TEAM_ID"

NOTARY_ARGS=()
ASC_TMP=""
cleanup_key() { [ -n "$ASC_TMP" ] && rm -rf "$(dirname "$ASC_TMP")"; }
trap cleanup_key EXIT

if [ "$NO_ASC" = 0 ]; then
    ASC_TMP="$(mktemp -d)/AuthKey.p8"
    ( umask 077; : > "$ASC_TMP" )
    if [ -n "$ASC_KEY_B64" ]; then
        printf '%s' "$ASC_KEY_B64" | base64 --decode > "$ASC_TMP" 2>/dev/null || : > "$ASC_TMP"
    elif [ -n "$ASC_KEY_PATH" ] && [ -f "$ASC_KEY_PATH" ]; then
        cat "$ASC_KEY_PATH" > "$ASC_TMP"
    else
        RAW="$(security find-generic-password -s "$ASC_KEY_SERVICE" -a "$ASC_KEY_ID" -w 2>/dev/null || true)"
        if [ -n "$RAW" ]; then
            case "$RAW" in
                *BEGIN*) printf '%s\n' "$RAW" > "$ASC_TMP" ;;
                *)       printf '%s' "$RAW" | xxd -r -p > "$ASC_TMP" 2>/dev/null || : > "$ASC_TMP" ;;
            esac
        fi
    fi
    if grep -q "BEGIN PRIVATE KEY" "$ASC_TMP" 2>/dev/null; then
        info "notary     App Store Connect API key $ASC_KEY_ID"
        NOTARY_ARGS=( --key "$ASC_TMP" --key-id "$ASC_KEY_ID" --issuer "$ASC_ISSUER_ID" )
    else
        cleanup_key; ASC_TMP=""
    fi
fi

if [ ${#NOTARY_ARGS[@]} -eq 0 ]; then
    [ -n "$KEYCHAIN_SERVICE" ] || die "no keychain service for '$IDENTITY_PRESET'"
    [ -n "$APPLE_ID_USED" ]    || die "no Apple ID for '$IDENTITY_PRESET'"
    NOTARY_PASSWORD="$(security find-generic-password -s "$KEYCHAIN_SERVICE" -a "$APPLE_ID_USED" -w 2>/dev/null || true)"
    [ -n "$NOTARY_PASSWORD" ] || die "no notarisation credential found"
    info "notary     Apple ID $APPLE_ID_USED (app-specific password)"
    NOTARY_ARGS=( --apple-id "$APPLE_ID_USED" --team-id "$TEAM_ID" --password "$NOTARY_PASSWORD" )
fi

submit() {
    info "notarizing $2 (this usually takes a minute or two)"
    xcrun notarytool submit "$1" "${NOTARY_ARGS[@]}" --wait 2>&1 | sed 's/^/           /'
}

case "$SIGN_TARGET" in
    *.dmg|*.pkg)
        submit "$SIGN_TARGET" "$(basename "$SIGN_TARGET")"
        ;;
    *)
        ZIP="$(mktemp -d)/$(basename "$SIGN_TARGET").zip"
        ditto -c -k --keepParent "$SIGN_TARGET" "$ZIP"
        submit "$ZIP" "$(basename "$SIGN_TARGET")"
        rm -rf "$(dirname "$ZIP")"
        ;;
esac

case "$SIGN_TARGET" in
    *.app|*.dmg|*.pkg)
        info "stapling   $(basename "$SIGN_TARGET")"
        xcrun stapler staple "$SIGN_TARGET" 2>&1 | sed 's/^/           /'
        xcrun stapler validate "$SIGN_TARGET" 2>&1 | sed 's/^/           /'
        ;;
esac

info "done: $SIGN_TARGET"
