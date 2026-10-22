#!/usr/bin/env bash
#
# Build script for Amar Bangla Kotha macOS Input Method Bundle (.app)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
APP_BUNDLE="$BUILD_DIR/AmarBanglaKotha.app"
MACOS_DIR="$APP_BUNDLE/Contents/MacOS"
RESOURCES_DIR="$APP_BUNDLE/Contents/Resources"

echo "=== Building Amar Bangla Kotha for macOS ==="

# 1. Ensure C++ library is built
echo "1. Building core static library..."
cd "$ROOT_DIR"
make lib/libabk.a

# 2. Prepare bundle directory hierarchy
echo "2. Creating .app bundle structure..."
rm -rf "$APP_BUNDLE"
mkdir -p "$MACOS_DIR"
mkdir -p "$RESOURCES_DIR"

# 3. Copy metadata and dictionary resources
echo "3. Copying Info.plist and dictionary..."
cp "$SCRIPT_DIR/Info.plist" "$APP_BUNDLE/Contents/Info.plist"
cp "$ROOT_DIR/data/bangla_wordlist.txt" "$RESOURCES_DIR/"
cp "$ROOT_DIR/data/phonetic_rules.tsv" "$RESOURCES_DIR/"

# 4. Compile Objective-C InputMethodKit binary
echo "4. Compiling binary..."
CLANG=${CLANG:-clang}

$CLANG -O3 -std=c++17 -ObjC++ \
    -I"$ROOT_DIR/include" \
    -F/System/Library/Frameworks \
    -framework Cocoa \
    -framework InputMethodKit \
    "$SCRIPT_DIR/main.m" \
    "$SCRIPT_DIR/AmarBanglaKothaController.m" \
    "$ROOT_DIR/lib/libabk.a" \
    -lc++ \
    -o "$MACOS_DIR/AmarBanglaKotha"

# 5. Ad-hoc codesign for local testing
if command -v codesign &>/dev/null; then
    echo "5. Ad-hoc code signing..."
    codesign --force --deep --sign - "$APP_BUNDLE"
fi

echo "========================================================="
echo " Build succeeded!"
echo " Output bundle: $APP_BUNDLE"
echo " To install on macOS:"
echo "   cp -R '$APP_BUNDLE' ~/Library/Input\\ Methods/"
echo " Then log out or restart to register with macOS Input Sources."
echo "========================================================="
