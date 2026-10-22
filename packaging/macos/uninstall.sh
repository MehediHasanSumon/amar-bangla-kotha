#!/bin/bash
set -e

# Clean uninstaller for Amar Bangla Kotha on macOS
echo "Uninstalling Amar Bangla Kotha from macOS..."

# 1. Terminate running process
killall -9 AmarBanglaKotha 2>/dev/null || true

# 2. Remove app bundle from /Library/Input Methods
if [ -d "/Library/Input Methods/AmarBanglaKotha.app" ]; then
    echo "Removing /Library/Input Methods/AmarBanglaKotha.app..."
    sudo rm -rf "/Library/Input Methods/AmarBanglaKotha.app"
fi

# 3. Remove user-level app bundle if present
if [ -d "$HOME/Library/Input Methods/AmarBanglaKotha.app" ]; then
    echo "Removing $HOME/Library/Input Methods/AmarBanglaKotha.app..."
    rm -rf "$HOME/Library/Input Methods/AmarBanglaKotha.app"
fi

# 4. Touch directory to notify system
touch "/Library/Input Methods" 2>/dev/null || true

# 5. Clean package receipt
sudo pkgutil --forget org.amar.bangla.kotha.inputmethod 2>/dev/null || true

echo "Amar Bangla Kotha has been cleanly uninstalled."
