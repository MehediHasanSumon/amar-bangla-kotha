# Code Signing & Notarization Guide

This document outlines the code signing and verification procedures for **Amar Bangla Kotha** across Windows and macOS platforms, ensuring safe, untampered distribution without browser or OS warnings (such as Windows SmartScreen or macOS Gatekeeper).

---

## 1. Overview & Security Principles

- **No Fabricated Certificates**: We never commit mock or fake certificates into the repository.
- **Reproducible Pipeline**: All signing and notarization steps are driven via [`packaging/scripts/sign_release.sh`](../packaging/scripts/sign_release.sh) and [`.github/workflows/release.yml`](../.github/workflows/release.yml).
- **Graceful Fallback**: If signing secrets are not present in the environment, the build pipeline logs an informative warning and completes unsigned builds without breaking CI/CD.

---

## 2. Windows Code Signing (Authenticode)

### Required Certificate Type
- Standard or Extended Validation (EV) Code Signing Certificate from a trusted Public Certificate Authority (DigiCert, Sectigo, GlobalSign, SSL.com, or Microsoft Azure Trusted Signing).
- Format: PKCS#12 (`.pfx` or `.p12`) or hardware token / Cloud HSM.

### Required Environment / GitHub Secrets
To enable Windows signing in GitHub Actions or locally, configure the following:

| Secret Name | Description | Example / Default |
|-------------|-------------|-------------------|
| `WIN_CERT_FILE` | Absolute path or base64-decoded path to the `.pfx` file | `/path/to/my_authenticode_cert.pfx` |
| `WIN_CERT_PASSWORD` | Password protecting the `.pfx` private key | `P@ssw0rd123!` |
| `WIN_TIMESTAMP_URL` | RFC 3161 Timestamp Authority URL (Optional) | `http://timestamp.digicert.com` |

### Manual Local Signing (Windows)
```cmd
signtool.exe sign /f "cert.pfx" /p "%WIN_CERT_PASSWORD%" /fd SHA256 /tr "http://timestamp.digicert.com" /td SHA256 bin\AmarBanglaKotha-Setup-1.0.0.exe
signtool.exe verify /pa /v bin\AmarBanglaKotha-Setup-1.0.0.exe
```

### Manual Cross-Platform Signing (Linux with `osslsigncode`)
```bash
osslsigncode sign -pkcs12 cert.pfx -pass "$WIN_CERT_PASSWORD" \
    -h sha256 -t http://timestamp.digicert.com \
    -in bin/AmarBanglaKotha-Setup-1.0.0.exe \
    -out bin/AmarBanglaKotha-Setup-1.0.0-signed.exe
```

---

## 3. macOS Code Signing & Apple Notarization

### Required Apple Developer Credentials
1. Active Apple Developer Program enrollment ($99/year).
2. **Developer ID Application Certificate**: Used to sign Mach-O binaries and the `.app` bundle.
3. **Developer ID Installer Certificate**: Used to sign the `.pkg` distribution installer.
4. **App-Specific Password**: Generated from [appleid.apple.com](https://appleid.apple.com) for CLI notarization access.

### Required Environment / GitHub Secrets

| Secret Name | Description | Example |
|-------------|-------------|---------|
| `MACOS_SIGNING_IDENTITY` | Developer ID Application common name | `"Developer ID Application: Your Name (10CHARTEAM)"` |
| `MACOS_INSTALLER_IDENTITY` | Developer ID Installer common name | `"Developer ID Installer: Your Name (10CHARTEAM)"` |
| `APPLE_ID` | Apple Developer Account email | `developer@example.com` |
| `APPLE_APP_SPECIFIC_PASSWORD` | Generated App-Specific Password | `abcd-efgh-ijkl-mnop` |
| `APPLE_TEAM_ID` | 10-character Apple Developer Team ID | `AB12CD34EF` |

### Manual Local Signing & Notarization (macOS)

#### Step 1: Sign the `.app` Bundle
```bash
codesign --force --deep --verify --verbose \
    --sign "Developer ID Application: Your Name (TEAM_ID)" \
    --options runtime \
    --timestamp \
    build/macos_pkg_stage/AmarBanglaKotha.app
```

#### Step 2: Sign the `.pkg` Installer
```bash
productsign --sign "Developer ID Installer: Your Name (TEAM_ID)" \
    bin/AmarBanglaKotha-1.0.0.pkg \
    bin/AmarBanglaKotha-1.0.0-signed.pkg
```

#### Step 3: Submit to Apple Notary Service
```bash
xcrun notarytool submit bin/AmarBanglaKotha-1.0.0-signed.pkg \
    --apple-id "$APPLE_ID" \
    --password "$APPLE_APP_SPECIFIC_PASSWORD" \
    --team-id "$APPLE_TEAM_ID" \
    --wait
```

#### Step 4: Staple the Ticket
```bash
xcrun stapler staple bin/AmarBanglaKotha-1.0.0-signed.pkg
```

#### Step 5: Verify Gatekeeper Compliance
```bash
spctl -a -vvv -t install bin/AmarBanglaKotha-1.0.0-signed.pkg
```
Expected output: `source=Notarized Developer ID`

---

## 4. Verification Checklist Before Release

- [ ] Windows `.exe` installer passes `signtool verify /pa`.
- [ ] Windows SmartScreen reputation established or signed with EV certificate.
- [ ] macOS `.pkg` passes `spctl -a -vvv -t install`.
- [ ] macOS app launches without Gatekeeper warnings on fresh macOS install.
- [ ] SHA-256 checksums recorded and published alongside release assets.
