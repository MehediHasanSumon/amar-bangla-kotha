#!/bin/bash
set -euo pipefail

# Automated Code Signing & Notarization Script
# Amar Bangla Kotha v1.0
#
# This script handles code signing for:
#  1. Windows (.exe / .dll) via SignTool or osslsigncode
#  2. macOS (.app / .pkg) via codesign, productsign, and notarytool
#
# Required Secrets (pass via environment variables or CI secrets):
#   - Windows:
#       WIN_CERT_FILE (Path to .pfx certificate file)
#       WIN_CERT_PASSWORD (Password for the .pfx certificate)
#       WIN_TIMESTAMP_URL (Default: http://timestamp.digicert.com)
#   - macOS:
#       MACOS_SIGNING_IDENTITY (e.g., "Developer ID Application: Your Name (TEAM_ID)")
#       MACOS_INSTALLER_IDENTITY (e.g., "Developer ID Installer: Your Name (TEAM_ID)")
#       APPLE_ID (Apple ID email for notarization)
#       APPLE_APP_SPECIFIC_PASSWORD (App-specific password for notarization)
#       APPLE_TEAM_ID (10-character Apple Developer Team ID)

TARGET_PLATFORM="${1:-}"
TARGET_FILE="${2:-}"

if [ -z "$TARGET_PLATFORM" ] || [ -z "$TARGET_FILE" ]; then
    echo "Usage: $0 <windows|macos> <target-file-path>"
    exit 1
fi

if [ ! -e "$TARGET_FILE" ]; then
    echo "Error: Target file '$TARGET_FILE' does not exist."
    exit 1
fi

echo "=========================================================="
echo "Signing Target: $TARGET_FILE on platform: $TARGET_PLATFORM"
echo "=========================================================="

case "$TARGET_PLATFORM" in
    windows)
        TIMESTAMP_URL="${WIN_TIMESTAMP_URL:-http://timestamp.digicert.com}"
        if [ -z "${WIN_CERT_FILE:-}" ] || [ -z "${WIN_CERT_PASSWORD:-}" ]; then
            echo "[WARNING] WIN_CERT_FILE or WIN_CERT_PASSWORD is not set."
            echo "Skipping Windows Authenticode signing (Dry-run / Development mode)."
            echo "See docs/CODE_SIGNING.md for details on configuring production certificates."
            exit 0
        fi

        if command -v signtool.exe >/dev/null 2>&1; then
            echo "Signing with Windows signtool.exe..."
            signtool.exe sign /f "$WIN_CERT_FILE" /p "$WIN_CERT_PASSWORD" /fd SHA256 /tr "$TIMESTAMP_URL" /td SHA256 "$TARGET_FILE"
            signtool.exe verify /pa /v "$TARGET_FILE"
        elif command -v osslsigncode >/dev/null 2>&1; then
            echo "Signing with cross-platform osslsigncode..."
            TMP_OUT="${TARGET_FILE}.signed"
            osslsigncode sign -pkcs12 "$WIN_CERT_FILE" -pass "$WIN_CERT_PASSWORD" -h sha256 -t "$TIMESTAMP_URL" -in "$TARGET_FILE" -out "$TMP_OUT"
            mv "$TMP_OUT" "$TARGET_FILE"
            echo "Windows binary signed successfully with osslsigncode."
        else
            echo "Error: Neither 'signtool.exe' nor 'osslsigncode' found in PATH."
            exit 1
        fi
        ;;

    macos)
        if [ -z "${MACOS_SIGNING_IDENTITY:-}" ]; then
            echo "[WARNING] MACOS_SIGNING_IDENTITY is not set."
            echo "Skipping macOS code signing and Apple notarization (Dry-run mode)."
            echo "See docs/CODE_SIGNING.md for setup instructions."
            exit 0
        fi

        if ! command -v codesign >/dev/null 2>&1; then
            echo "Error: 'codesign' command is only available on macOS."
            exit 1
        fi

        echo "1. Code signing application bundle..."
        codesign --force --deep --verify --verbose \
            --sign "$MACOS_SIGNING_IDENTITY" \
            --options runtime \
            --timestamp \
            "$TARGET_FILE"

        codesign --verify --deep --strict --verbose=2 "$TARGET_FILE"

        # If it's a .pkg installer, sign with installer identity and notarize
        if [[ "$TARGET_FILE" == *.pkg ]] && [ -n "${MACOS_INSTALLER_IDENTITY:-}" ]; then
            SIGNED_PKG="${TARGET_FILE%.pkg}-signed.pkg"
            echo "2. Signing installer package..."
            productsign --sign "$MACOS_INSTALLER_IDENTITY" "$TARGET_FILE" "$SIGNED_PKG"
            mv "$SIGNED_PKG" "$TARGET_FILE"

            # Apple Notarization
            if [ -n "${APPLE_ID:-}" ] && [ -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ] && [ -n "${APPLE_TEAM_ID:-}" ]; then
                echo "3. Submitting package to Apple Notary Service..."
                xcrun notarytool submit "$TARGET_FILE" \
                    --apple-id "$APPLE_ID" \
                    --password "$APPLE_APP_SPECIFIC_PASSWORD" \
                    --team-id "$APPLE_TEAM_ID" \
                    --wait

                echo "4. Stapling notarization ticket to package..."
                xcrun stapler staple "$TARGET_FILE"
                echo "Apple notarization and stapling complete."
            else
                echo "[WARNING] APPLE_ID, APPLE_APP_SPECIFIC_PASSWORD, or APPLE_TEAM_ID not provided. Skipping notarization."
            fi
        fi
        ;;

    *)
        echo "Error: Unsupported platform '$TARGET_PLATFORM'. Use 'windows' or 'macos'."
        exit 1
        ;;
esac

echo "Code signing step completed successfully."
