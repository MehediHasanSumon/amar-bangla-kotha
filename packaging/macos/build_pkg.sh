#!/bin/bash
set -euo pipefail

# Build script for macOS .pkg installer (Amar Bangla Kotha)
# Usage: ./packaging/macos/build_pkg.sh [version]

VERSION="${1:-1.0.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUTPUT_DIR="${ROOT_DIR}/bin"
STAGE_DIR="${ROOT_DIR}/build/macos_pkg_stage"
APP_BUNDLE="${STAGE_DIR}/AmarBanglaKotha.app"
PKG_IDENTIFIER="org.amar.bangla.kotha.inputmethod"
PKG_OUTPUT="${OUTPUT_DIR}/AmarBanglaKotha-${VERSION}.pkg"

echo "=== Building macOS Package for Amar Bangla Kotha v${VERSION} ==="

mkdir -p "${OUTPUT_DIR}"
rm -rf "${STAGE_DIR}"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources/data"

# Copy Info.plist
cp "${SCRIPT_DIR}/Info.plist" "${APP_BUNDLE}/Contents/Info.plist"
# Update version in Info.plist
sed -i "s/<string>1.0.0<\/string>/<string>${VERSION}<\/string>/g" "${APP_BUNDLE}/Contents/Info.plist" 2>/dev/null || true

# Copy dictionary and bigram datasets
cp "${ROOT_DIR}/data/bangla_wordlist.txt" "${APP_BUNDLE}/Contents/Resources/data/"
cp "${ROOT_DIR}/data/bangla_bigrams.tsv" "${APP_BUNDLE}/Contents/Resources/data/"
cp "${ROOT_DIR}/tests/data/golden.tsv" "${APP_BUNDLE}/Contents/Resources/data/"

# Copy Settings GUI & tutorial
cp "${ROOT_DIR}/tools/abk_settings.py" "${APP_BUNDLE}/Contents/Resources/"

# Copy binary / executable if built, or create stub launcher script
if [ -f "${ROOT_DIR}/bin/AmarBanglaKotha" ]; then
    cp "${ROOT_DIR}/bin/AmarBanglaKotha" "${APP_BUNDLE}/Contents/MacOS/AmarBanglaKotha"
elif [ -f "${ROOT_DIR}/build/AmarBanglaKotha" ]; then
    cp "${ROOT_DIR}/build/AmarBanglaKotha" "${APP_BUNDLE}/Contents/MacOS/AmarBanglaKotha"
else
    # Create wrapper launcher script if building cross-platform
    cat << 'EOF' > "${APP_BUNDLE}/Contents/MacOS/AmarBanglaKotha"
#!/bin/bash
DIR="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "${DIR}/Resources/abk_settings.py" "$@"
EOF
    chmod +x "${APP_BUNDLE}/Contents/MacOS/AmarBanglaKotha"
fi

# Check if pkgbuild is available (running on macOS)
if command -v pkgbuild >/dev/null 2>&1; then
    echo "Running pkgbuild on macOS..."
    pkgbuild \
        --root "${STAGE_DIR}" \
        --identifier "${PKG_IDENTIFIER}" \
        --version "${VERSION}" \
        --install-location "/Library/Input Methods" \
        --scripts "${SCRIPT_DIR}/scripts" \
        "${PKG_OUTPUT}"
    echo "Successfully generated: ${PKG_OUTPUT}"
else
    echo "Notice: 'pkgbuild' is only available natively on macOS."
    echo "Created macOS App Bundle staging layout at: ${APP_BUNDLE}"
    echo "Creating archive: ${OUTPUT_DIR}/AmarBanglaKotha-${VERSION}-macos.tar.gz"
    tar -czf "${OUTPUT_DIR}/AmarBanglaKotha-${VERSION}-macos.tar.gz" -C "${STAGE_DIR}" AmarBanglaKotha.app
    echo "Staged macOS package bundle created successfully."
fi
