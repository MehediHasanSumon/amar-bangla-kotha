# Packaging & Distribution Guide (v1.0)

This guide documents the installation, building, and packaging workflows for **Amar Bangla Kotha** across Linux, Windows, and macOS.

---

## 1. Linux Packaging (.deb, RPM, Flatpak)

### Debian / Ubuntu (`.deb`)
The primary packaging format for Linux is a standard Debian package (`.deb`).

#### Building the `.deb` package
Run the Makefile target from the repository root:
```bash
make package-deb
```
This executes [`packaging/linux/build_deb.sh`](../packaging/linux/build_deb.sh), which:
1. Compiles the core engine shared library (`libabk.so`) and CLI diagnostic tool (`abk_cli`).
2. Stages the standard directory hierarchy:
   - `/usr/lib/libabk.so` (Shared library)
   - `/usr/bin/abk_cli` (Diagnostic CLI)
   - `/usr/bin/abk-settings` (Settings GUI & First-Run Tutorial launcher)
   - `/usr/share/amar-bangla-kotha/` (Dictionary corpora, bigrams, assets)
   - `/usr/share/applications/amar-bangla-kotha.desktop` (Application menu entry)
3. Generates Debian control files with maintainer scripts:
   - `postinst`: Sets permissions, updates desktop database, and triggers IBus restart.
   - `prerm` / `postrm`: Cleanly shuts down active processes and removes runtime cache.
4. Produces `bin/amar-bangla-kotha_<version>_amd64.deb`.

#### Installation & Uninstallation
```bash
# Install
sudo dpkg -i bin/amar-bangla-kotha_1.0.0_amd64.deb
sudo apt-get install -f   # Installs any missing dependencies

# Uninstall cleanly
sudo apt-get remove amar-bangla-kotha
sudo apt-get purge amar-bangla-kotha
```

### Future Linux Formats

#### 1. Fedora / RHEL / openSUSE (`.rpm`)
- Planned for v1.1.
- Can be generated directly from `build/deb_stage` using `fpm` or standard `rpmbuild` with an `.rpmmacros` and `.spec` file targeting `/usr/lib64/libabk.so`.

#### 2. Flatpak
- Sandboxed desktop distribution targeting Flathub.
- Requires `--filesystem=xdg-config/amar-bangla-kotha` permissions and standard D-Bus / IBus integration portal.

---

## 2. Windows Packaging (Inno Setup .exe)

### Overview
Windows uses an Inno Setup 6 installer script located at [`packaging/windows/installer.iss`](../packaging/windows/installer.iss).

#### Features
- **One-Click Installation**: Installs to `C:\Program Files\AmarBanglaKotha`.
- **TSF COM Registration**: Automatically registers the Text Service Provider COM DLL via `regsvr32.exe /s "{app}\abk_tsf.dll"`.
- **First-Run Tutorial Prompt**: Offers an immediate interactive tutorial upon finishing setup.
- **Clean Uninstallation**: Automatically unregisters the COM component using `regsvr32.exe /u /s "{app}\abk_tsf.dll"` and cleans up shortcuts.

#### Building on Windows
Prerequisites: Inno Setup 6 installed (`iscc.exe` in PATH).
```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
iscc packaging\windows\installer.iss
```
The installer output will be generated at `bin\AmarBanglaKotha-Setup-1.0.0.exe`.

---

## 3. macOS Packaging (InputMethodKit .pkg)

### Overview
macOS input methods are packaged as application bundles placed in `/Library/Input Methods/` or `~/Library/Input Methods/`.

#### Bundle Structure
```text
AmarBanglaKotha.app/
├── Contents/
│   ├── Info.plist                     (InputMethodKit metadata, LSUIElement=1)
│   ├── MacOS/
│   │   └── AmarBanglaKotha            (Launcher binary / daemon)
│   └── Resources/
│       ├── abk_settings.py            (Bilingual GUI & Tutorial)
│       └── data/
│           ├── bangla_wordlist.txt
│           ├── bangla_bigrams.tsv
│           └── golden.tsv
```

#### Building the Package
```bash
./packaging/macos/build_pkg.sh 1.0.0
```
On macOS hosts, this executes `pkgbuild` with the post-install script at [`packaging/macos/scripts/postinstall`](../packaging/macos/scripts/postinstall). On Linux/Windows builders, it produces a staging archive `AmarBanglaKotha-1.0.0-macos.tar.gz`.

#### Uninstallation (macOS)
Run the provided clean uninstallation script:
```bash
sudo ./packaging/macos/uninstall.sh
```

---

## 4. Packaging Verification Matrix

| Target Platform | Package Format | Installer Script | Registration Mechanism | Clean Uninstaller Verified |
|---|---|---|---|---|
| **Linux (Debian/Ubuntu)** | `.deb` | [`packaging/linux/build_deb.sh`](../packaging/linux/build_deb.sh) | Desktop entry & IBus trigger | Yes (`apt purge`) |
| **Windows 10/11** | `.exe` | [`packaging/windows/installer.iss`](../packaging/windows/installer.iss) | `regsvr32 abk_tsf.dll` (TSF) | Yes (Windows Settings / Uninstall) |
| **macOS (Big Sur+)** | `.pkg` | [`packaging/macos/build_pkg.sh`](../packaging/macos/build_pkg.sh) | `/Library/Input Methods/` | Yes (`uninstall.sh`) |
