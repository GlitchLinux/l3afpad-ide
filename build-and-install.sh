#!/bin/bash
# l3afpad-ide source installer
# Builds from source and installs system-wide. Run from the extracted dir.
set -e

echo "== l3afpad-ide installer =="

# --- dependencies ---
if command -v apt-get >/dev/null 2>&1; then
    echo "Installing build dependencies (apt)..."
    sudo apt-get update
    sudo apt-get install -y build-essential autoconf automake intltool pkg-config \
        libgtk-3-dev libgtksourceview-4-dev libjson-glib-dev
else
    echo "Non-apt system: ensure these are installed:"
    echo "  gcc make autoconf automake intltool pkg-config"
    echo "  gtk+-3.0, gtksourceview-4, json-glib-1.0 (dev packages)"
fi

# --- build ---
echo "Building..."
./autogen.sh
./configure
make -j"$(nproc)"

# --- install ---
echo "Installing (sudo)..."
sudo make install
sudo gtk-update-icon-cache -f -t /usr/local/share/icons/hicolor 2>/dev/null || true
sudo update-desktop-database 2>/dev/null || true

echo ""
echo "Done. Run 'l3afpad' to start (installed to /usr/local/bin)."
echo "Themes: /usr/local/share/l3afpad/{themes.tsv,schemes/}"
