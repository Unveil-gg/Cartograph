#!/bin/bash
# -----------------------------------------------------------------------------
# codesign.sh - Sign Cartograph.app for macOS distribution
# -----------------------------------------------------------------------------
# Usage: ./scripts/codesign.sh [path/to/Cartograph.app]
#
# Prerequisites:
# - Apple Developer account with "Developer ID Application" certificate
# - Certificate installed in Keychain
#
# Environment variables (optional):
# - CODESIGN_IDENTITY: Signing identity (default: auto-detect Developer ID)
# - ENTITLEMENTS: Path to entitlements file (default: ./Cartograph.entitlements)
# -----------------------------------------------------------------------------

set -e

# Determine script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# App bundle path (default to build directory)
APP_PATH="${1:-$PROJECT_ROOT/build/Cartograph.app}"

if [ ! -d "$APP_PATH" ]; then
    echo "Error: App bundle not found at $APP_PATH"
    echo "Usage: $0 [path/to/Cartograph.app]"
    exit 1
fi

# Signing identity
if [ -z "$CODESIGN_IDENTITY" ]; then
    # Try to auto-detect Developer ID certificate
    CODESIGN_IDENTITY=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk -F'"' '{print $2}')
    
    if [ -z "$CODESIGN_IDENTITY" ]; then
        echo "Error: No Developer ID Application certificate found."
        echo "Install your certificate or set CODESIGN_IDENTITY environment variable."
        echo ""
        echo "For ad-hoc signing (development only):"
        echo "  CODESIGN_IDENTITY='-' $0 $APP_PATH"
        exit 1
    fi
fi

# Entitlements file
ENTITLEMENTS="${ENTITLEMENTS:-$PROJECT_ROOT/Cartograph.entitlements}"

if [ ! -f "$ENTITLEMENTS" ]; then
    echo "Warning: Entitlements file not found at $ENTITLEMENTS"
    echo "Proceeding without entitlements..."
    ENTITLEMENTS_ARG=""
else
    ENTITLEMENTS_ARG="--entitlements $ENTITLEMENTS"
fi

echo "=== Cartograph Code Signing ==="
echo "App:         $APP_PATH"
echo "Identity:    $CODESIGN_IDENTITY"
echo "Entitlements: ${ENTITLEMENTS:-none}"
echo ""

# Remove any existing signatures
echo "Removing existing signatures..."
codesign --remove-signature "$APP_PATH" 2>/dev/null || true

# Sign all nested frameworks and libraries first (if any)
echo "Signing nested components..."
find "$APP_PATH/Contents/Frameworks" -type f -name "*.dylib" 2>/dev/null | while read lib; do
    echo "  Signing: $lib"
    codesign --force --options runtime --timestamp \
        --sign "$CODESIGN_IDENTITY" "$lib"
done

# Sign the main executable
echo "Signing main executable..."
codesign --force --options runtime --timestamp \
    $ENTITLEMENTS_ARG \
    --sign "$CODESIGN_IDENTITY" \
    "$APP_PATH"

# Verify signature
echo ""
echo "Verifying signature..."
codesign --verify --verbose=2 "$APP_PATH"

# Check for Gatekeeper acceptance
echo ""
echo "Checking Gatekeeper acceptance..."
spctl --assess --type exec --verbose "$APP_PATH" || {
    echo ""
    echo "Note: Gatekeeper check failed. This is expected for:"
    echo "  - Ad-hoc signed apps (development)"
    echo "  - Apps not yet notarized"
    echo ""
    echo "For distribution, run notarize.sh after signing."
}

echo ""
echo "=== Signing Complete ==="
