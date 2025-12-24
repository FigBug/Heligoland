#!/bin/bash
set -e

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR/../.."

# Read version
VERSION=$(cat "$PROJECT_DIR/VERSION" | tr -d '[:space:]')

# Paths
BUILD_DIR="$PROJECT_DIR/build"
DEB_DIR="$BUILD_DIR/deb"
PKG_NAME="heligoland_${VERSION}_amd64"

echo "Building .deb package for Heligoland $VERSION..."

# Check executable exists
if [ ! -f "$BUILD_DIR/Heligoland" ]; then
    echo "Error: $BUILD_DIR/Heligoland not found. Build the project first."
    exit 1
fi

# Clean and create directory structure
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/$PKG_NAME/DEBIAN"
mkdir -p "$DEB_DIR/$PKG_NAME/usr/bin"
mkdir -p "$DEB_DIR/$PKG_NAME/usr/share/heligoland/assets"
mkdir -p "$DEB_DIR/$PKG_NAME/usr/share/applications"

# Create control file
cat > "$DEB_DIR/$PKG_NAME/DEBIAN/control" << EOF
Package: heligoland
Version: $VERSION
Section: games
Priority: optional
Architecture: amd64
Depends: libc6, libgl1, libasound2
Maintainer: Heligoland <heligoland@example.com>
Description: A naval combat game
 Heligoland is a local multiplayer battleship game where up to 4 players
 command warships in naval combat.
EOF

# Copy files
cp "$BUILD_DIR/Heligoland" "$DEB_DIR/$PKG_NAME/usr/bin/"
cp -r "$PROJECT_DIR/assets/"* "$DEB_DIR/$PKG_NAME/usr/share/heligoland/assets/"

# Create desktop file
cat > "$DEB_DIR/$PKG_NAME/usr/share/applications/heligoland.desktop" << EOF
[Desktop Entry]
Name=Heligoland
Comment=A naval combat game
Exec=/usr/bin/Heligoland
Terminal=false
Type=Application
Categories=Game;
EOF

# Set permissions
chmod 755 "$DEB_DIR/$PKG_NAME/usr/bin/Heligoland"

# Build package
dpkg-deb --build "$DEB_DIR/$PKG_NAME"

# Move to build directory
mv "$DEB_DIR/$PKG_NAME.deb" "$BUILD_DIR/"

# Clean up
rm -rf "$DEB_DIR"

echo "Created: $BUILD_DIR/$PKG_NAME.deb"
