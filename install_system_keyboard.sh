#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "  Amar Bangla Kotha (আমার বাংলা কথা) - System Keyboard Setup"
echo "=========================================================="
echo ""
echo "This will install Amar Bangla Kotha as a native OS keyboard"
echo "so you can type directly into Chrome, Text Editor, and all apps."
echo ""

# Ensure engine library is compiled and installed
cd "$(dirname "$0")"
make install-user

# Prompt for sudo to copy the IBus component to system directory
echo ""
echo "Updating system daemon..."
sudo cp bindings/linux/abk_ibus_engine.py /usr/local/bin/abk-ibus-daemon
sudo chmod +x /usr/local/bin/abk-ibus-daemon

echo "Restarting daemon..."
pkill -f abk-ibus-daemon || true
ibus restart >/dev/null 2>&1 || true
sleep 1

# Automatically add Amar Bangla Kotha to GNOME Input Sources
echo "Adding Amar Bangla Kotha to your desktop input sources..."
CURRENT_SOURCES=$(gsettings get org.gnome.desktop.input-sources sources)
if [[ "$CURRENT_SOURCES" != *"amar-bangla-kotha"* ]]; then
    # Add to list
    gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us'), ('ibus', 'amar-bangla-kotha')]"
fi

echo ""
echo "=========================================================="
echo "  INSTALLATION SUCCESSFUL! 🎉"
echo "=========================================================="
echo "Now you can type directly into ANY application (Chrome, Gedit, etc.):"
echo ""
echo "  1. Open Chrome or Text Editor."
echo "  2. Press: Super (Windows key) + Space  (or click top-right language icon)"
echo "  3. Select: 'Amar Bangla Kotha (আমার বাংলা কথা)'"
echo "  4. Type Banglish: 'ami', 'kotha', 'dhonnobad'..."
echo "     It will DIRECTLY convert to Bangla right where you type!"
echo "=========================================================="
