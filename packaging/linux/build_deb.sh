#!/bin/bash
set -e

# Build directory and metadata
VERSION="1.0.0"
ARCH=$(dpkg --print-architecture 2>/dev/null || echo "amd64")
PKG_NAME="amar-bangla-kotha_${VERSION}_${ARCH}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIST_DIR="${REPO_ROOT}/build/deb_staging"

echo "=========================================================="
echo " Packaging Amar Bangla Kotha for Linux Debian (${PKG_NAME}.deb)"
echo "=========================================================="

# 1. Clean staging directory
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}/DEBIAN"
mkdir -p "${DIST_DIR}/usr/lib"
mkdir -p "${DIST_DIR}/usr/include/abk"
mkdir -p "${DIST_DIR}/usr/bin"
mkdir -p "${DIST_DIR}/usr/share/ibus/component"
mkdir -p "${DIST_DIR}/usr/share/amar-bangla-kotha"
mkdir -p "${DIST_DIR}/usr/share/applications"

# 2. Build release binaries
cd "${REPO_ROOT}"
make all

# 3. Copy control files
cp "${REPO_ROOT}/packaging/linux/debian/control" "${DIST_DIR}/DEBIAN/"
cp "${REPO_ROOT}/packaging/linux/debian/postinst" "${DIST_DIR}/DEBIAN/"
cp "${REPO_ROOT}/packaging/linux/debian/prerm" "${DIST_DIR}/DEBIAN/"
cp "${REPO_ROOT}/packaging/linux/debian/postrm" "${DIST_DIR}/DEBIAN/"
chmod 755 "${DIST_DIR}/DEBIAN/postinst" "${DIST_DIR}/DEBIAN/prerm" "${DIST_DIR}/DEBIAN/postrm"

# 4. Copy shared library & headers
cp "${REPO_ROOT}/lib/libabk.so" "${DIST_DIR}/usr/lib/"
cp "${REPO_ROOT}/include/abk/"*.h "${DIST_DIR}/usr/include/abk/"

# 5. Copy binaries and frontend scripts
cp "${REPO_ROOT}/bin/abk_cli" "${DIST_DIR}/usr/bin/"
cp "${REPO_ROOT}/tools/abk_settings.py" "${DIST_DIR}/usr/bin/amar-bangla-kotha-settings"
chmod +x "${DIST_DIR}/usr/bin/amar-bangla-kotha-settings"
cp "${REPO_ROOT}/tools/abk_legacy_clipboard.py" "${DIST_DIR}/usr/bin/abk-legacy-clipboard"
chmod +x "${DIST_DIR}/usr/bin/abk-legacy-clipboard"

cp "${REPO_ROOT}/bindings/linux/abk_ibus_engine.py" "${DIST_DIR}/usr/bin/amar-bangla-kotha-ibus"
chmod +x "${DIST_DIR}/usr/bin/amar-bangla-kotha-ibus"

# 6. Copy IBus Component XML definition
cp "${REPO_ROOT}/bindings/linux/amar-bangla-kotha.xml" "${DIST_DIR}/usr/share/ibus/component/"

# 7. Copy Data & Evaluation corpora
cp "${REPO_ROOT}/data/bangla_wordlist.txt" "${DIST_DIR}/usr/share/amar-bangla-kotha/"
cp "${REPO_ROOT}/data/bangla_bigrams.tsv" "${DIST_DIR}/usr/share/amar-bangla-kotha/"
cp "${REPO_ROOT}/tests/data/golden.tsv" "${DIST_DIR}/usr/share/amar-bangla-kotha/"
mkdir -p "${DIST_DIR}/usr/share/amar-bangla-kotha/domains"
cp "${REPO_ROOT}/data/domains/"*.tsv "${DIST_DIR}/usr/share/amar-bangla-kotha/domains/"

# 8. Copy desktop entry
cp "${REPO_ROOT}/packaging/linux/amar-bangla-kotha.desktop" "${DIST_DIR}/usr/share/applications/"

# 9. Build Debian package
mkdir -p "${REPO_ROOT}/bin"
dpkg-deb --build --root-owner-group "${DIST_DIR}" "${REPO_ROOT}/bin/${PKG_NAME}.deb"

echo ""
echo "Debian package successfully built at:"
echo "  ${REPO_ROOT}/bin/${PKG_NAME}.deb"
echo "Package contents:"
dpkg-deb -c "${REPO_ROOT}/bin/${PKG_NAME}.deb"
