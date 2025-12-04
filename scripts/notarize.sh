#!/bin/bash
# -----------------------------------------------------------------------------
# notarize.sh - Notarize Cartograph.app for macOS Gatekeeper
# -----------------------------------------------------------------------------
# Usage: ./scripts/notarize.sh [path/to/Cartograph.app]
#
# Prerequisites:
# - App must be signed with Developer ID certificate (run codesign.sh first)
# - Apple Developer account with App Store Connect API access
# - App-specific password created at appleid.apple.com
#
# Environment variables (required):
# - APPLE_ID: Your Apple ID email
# - APPLE_TEAM_ID: Your 10-character Team ID
# - APPLE_APP_PASSWORD: App-specific password (NOT your Apple ID password)
#
# To create an app-specific password:
# 1. Go to appleid.apple.com
# 2. Sign in and go to Security > App-Specific Passwords
# 3. Generate a new password for "Cartograph Notarization"
# -----------------------------------------------------------------------------

set -e

# Determine script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# App bundle path
APP_PATH="${1:-$PROJECT_ROOT/build/Cartograph.app}"

if [ ! -d "$APP_PATH" ]; then
    echo "Error: App bundle not found at $APP_PATH"
    echo "Usage: $0 [path/to/Cartograph.app]"
    exit 1
fi

# Check required environment variables
if [ -z "$APPLE_ID" ]; then
    echo "Error: APPLE_ID environment variable not set"
    echo "Set it to your Apple ID email address"
    exit 1
fi

if [ -z "$APPLE_TEAM_ID" ]; then
    echo "Error: APPLE_TEAM_ID environment variable not set"
    echo "Set it to your 10-character Team ID (find at developer.apple.com)"
    exit 1
fi

if [ -z "$APPLE_APP_PASSWORD" ]; then
    echo "Error: APPLE_APP_PASSWORD environment variable not set"
    echo "Create an app-specific password at appleid.apple.com"
    exit 1
fi

# Create a temporary ZIP for notarization
APP_NAME=$(basename "$APP_PATH" .app)
ZIP_PATH="/tmp/${APP_NAME}_notarize.zip"

echo "=== Cartograph Notarization ==="
echo "App:     $APP_PATH"
echo "Apple ID: $APPLE_ID"
echo "Team ID:  $APPLE_TEAM_ID"
echo ""

# Verify the app is properly signed first
echo "Verifying code signature..."
codesign --verify --verbose=2 "$APP_PATH" || {
    echo "Error: App is not properly signed. Run codesign.sh first."
    exit 1
}

# Create ZIP archive
echo "Creating ZIP archive for upload..."
rm -f "$ZIP_PATH"
ditto -c -k --keepParent "$APP_PATH" "$ZIP_PATH"

# Submit for notarization
echo ""
echo "Submitting to Apple notarization service..."
echo "(This may take several minutes)"
echo ""

xcrun notarytool submit "$ZIP_PATH" \
    --apple-id "$APPLE_ID" \
    --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_PASSWORD" \
    --wait

# Clean up ZIP
rm -f "$ZIP_PATH"

# Staple the notarization ticket to the app
echo ""
echo "Stapling notarization ticket..."
xcrun stapler staple "$APP_PATH"

# Final verification
echo ""
echo "Verifying notarization..."
xcrun stapler validate "$APP_PATH"

# Gatekeeper check
echo ""
echo "Final Gatekeeper assessment..."
spctl --assess --type exec --verbose "$APP_PATH"

echo ""
echo "=== Notarization Complete ==="
echo ""
echo "The app is now ready for distribution!"
echo "Users can download and run it without Gatekeeper warnings."

