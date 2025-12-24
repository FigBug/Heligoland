#!/bin/bash
set -e

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR/../.."

# Read version
VERSION=$(cat "$PROJECT_DIR/VERSION" | tr -d '[:space:]')

# Paths
APP_PATH="$PROJECT_DIR/build/Heligoland.app"
PKG_OUTPUT="$PROJECT_DIR/build/Heligoland-${VERSION}.pkg"
COMPONENT_PKG="$PROJECT_DIR/build/Heligoland-component.pkg"

# Check app exists
if [ ! -d "$APP_PATH" ]; then
    echo "Error: $APP_PATH not found. Build the project first."
    exit 1
fi

echo "Building installer for Heligoland $VERSION..."

# Create component package
pkgbuild \
    --root "$APP_PATH" \
    --install-location "/Applications/Heligoland.app" \
    --identifier "com.heligoland.game" \
    --version "$VERSION" \
    "$COMPONENT_PKG"

# Create product archive (final installer)
productbuild \
    --package "$COMPONENT_PKG" \
    --identifier "com.heligoland.game.pkg" \
    --version "$VERSION" \
    "$PKG_OUTPUT"

# Clean up component package
rm "$COMPONENT_PKG"

echo "Created: $PKG_OUTPUT"
