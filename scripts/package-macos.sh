#!/usr/bin/env bash
# macOS package of a built LYNX.app (shown as LYNX SHARE: CFBundleName).
# When a Developer ID Application certificate is present in the keychain (CI or local), signs LYNX.app with Hardened
# Runtime + resources/LYNX.entitlements, builds the compressed .dmg, signs the .dmg, and (when notarisation credentials
# are present) notarizes and staples it. Otherwise falls back to an ad-hoc signature so local builds still run cleanly.
#
# usage: scripts/package-macos.sh <LYNX.app> <label> <output folder> [Demo.lynx]
set -euo pipefail
cd "$(dirname "$0")/.."

APP="$1"; LABEL="$2"; OUTDIR="$3"; DEMO="${4:-}"
NAME="LYNX-$LABEL-macos-universal"
STAGE="$(mktemp -d)/LYNX"
mkdir -p "$STAGE" "$OUTDIR"

ditto "$APP" "$STAGE/LYNX.app"
./scripts/fetch-onnxruntime.sh macos "$STAGE/LYNX.app/Contents/Frameworks"

CERT_LINE="$(security find-identity -v -p codesigning 2>/dev/null | grep -F "Developer ID Application" | head -1 || true)"
if [[ -n "$CERT_LINE" ]]; then
    echo "==> Signing LYNX.app with Developer ID Application"
    ./scripts/sign-macos.sh "$STAGE/LYNX.app" --dry-run
else
    # ad-hoc signature over the whole bundle: Apple Silicon refuses to run unsigned code, and a consistent bundle
    # signature turns "LYNX is damaged" into the normal "unidentified developer" prompt
    codesign --force --deep --sign - "$STAGE/LYNX.app"
    codesign --verify --deep --strict "$STAGE/LYNX.app"
fi

ln -s /Applications "$STAGE/Applications"
[[ -n "$DEMO" && -f "$DEMO" ]] && cp "$DEMO" "$STAGE/Demo.lynx"
cp LICENSE "$STAGE/LICENSE.txt"

if [[ -n "$CERT_LINE" ]]; then
    cat > "$STAGE/Read me first.txt" <<'EOF'
LYNX SHARE for macOS (Apple Silicon and Intel, macOS 11 or newer)

1. Drag LYNX SHARE into the Applications folder.
2. Open LYNX SHARE from Applications.
3. macOS asks for microphone access the first time you record.

Source code, checksums and help: https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO
EOF
else
    cat > "$STAGE/Read me first.txt" <<'EOF'
LYNX SHARE for macOS (Apple Silicon and Intel, macOS 11 or newer)

1. Drag LYNX SHARE into the Applications folder.
2. LYNX SHARE is free software from an independent developer without an Apple Developer ID, so it is not notarized by
   Apple. The first time, open it like this:
     - Control-click (or right-click) LYNX SHARE in Applications, choose Open, then Open again.
     - On macOS 15 Sequoia or newer: try to open LYNX SHARE once, then go to System Settings > Privacy & Security and
       click "Open Anyway" next to the message about LYNX.
   Or, in Terminal:   xattr -dr com.apple.quarantine /Applications/LYNX.app
   After that it opens normally.
3. macOS asks for microphone access the first time you record.

Source code, checksums and help: https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO
EOF
fi

rm -f "$OUTDIR/$NAME.dmg"
# hdiutil sometimes fails with "Resource busy" on CI runners (a transient lock on the image); retry a few times
for attempt in 1 2 3 4 5; do
    hdiutil create -volname "LYNX SHARE $LABEL" -srcfolder "$STAGE" -fs HFS+ -format UDZO -imagekey zlib-level=9 -ov \
        "$OUTDIR/$NAME.dmg" && break
    [[ $attempt -lt 5 ]] || exit 1
    echo "hdiutil create failed (attempt $attempt), retrying"; sleep $((attempt * 10))
done
hdiutil verify "$OUTDIR/$NAME.dmg"

if [[ -n "$CERT_LINE" ]]; then
    if [[ -n "${ASC_KEY_B64:-}" || -n "${ASC_KEY_PATH:-}" ]]; then
        echo "==> Signing, notarizing, and stapling $OUTDIR/$NAME.dmg"
        ./scripts/sign-macos.sh "$OUTDIR/$NAME.dmg"
    else
        echo "==> Signing $OUTDIR/$NAME.dmg (no notarisation key in environment)"
        ./scripts/sign-macos.sh "$OUTDIR/$NAME.dmg" --dry-run
    fi
fi

ls -la "$OUTDIR/$NAME.dmg"
