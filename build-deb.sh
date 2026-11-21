#!/bin/bash
# Build an installable .deb of the l3afpad remaster (sidebar fork).
# Produces: /FILES/GLITCH-SOFTWARE/l3afpad-remaster-2026-v0.3.deb
set -e

SRC="/FILES/GLITCH-SOFTWARE/l3afpad-linux"
WORK="/tmp/l3af-deb-build"
OUT="/FILES/GLITCH-SOFTWARE/l3afpad-remaster-2026-v0.3.deb"
PKGVER="0.8.18.1.11-remaster2026.0.3"

rm -rf "$WORK"
mkdir -p "$WORK"
# copy source tree (exclude VCS + prior build artifacts), build in /tmp
cp -a "$SRC" "$WORK/l3afpad-remaster"
cd "$WORK/l3afpad-remaster"
rm -rf .git autom4te.cache config.log config.status Makefile src/l3afpad \
       src/.deps src/*.o .shots 2>/dev/null || true

# --- rebrand the package so it does not clash with the stock l3afpad ---
# Keep the binary name 'l3afpad' (it's a drop-in), but ship as its own package.
sed -i 's/^Source: l3afpad/Source: l3afpad-remaster/' debian/control
sed -i 's/^Package: l3afpad/Package: l3afpad-remaster/' debian/control
# Make it replace/conflict with stock l3afpad since both install /usr/bin/l3afpad
if ! grep -q '^Conflicts:' debian/control; then
  sed -i '/^Depends: .*misc:Depends}/a Conflicts: l3afpad\nReplaces: l3afpad\nProvides: l3afpad' debian/control
fi

# --- changelog entry for the remaster version ---
export DEBEMAIL="x@glitchlinux.wtf"
export DEBFULLNAME="GlitchLinux"
dch --create --package l3afpad-remaster -v "$PKGVER" \
    --distribution unstable \
    "Remaster 2026 v0.3: JSON settings persistence (theme/font/sidebar remembered), custom icons, fixed theme submenus. v0.2: Themes menu. v0.1: sidebar file browser (text-file
    listing, folder navigation, single-buffer open). Sidebar extends the
    window outward. Files menu trimmed (no Statistics/Print)." \
    2>/dev/null || {
      # dch --create wants an empty changelog; prepend instead
      cat > /tmp/newch <<EOF
l3afpad-remaster ($PKGVER) unstable; urgency=medium

  * Remaster 2026 v0.3: JSON settings persistence (theme/font/sidebar remembered), custom icons, fixed theme submenus. v0.2: Themes menu. v0.1: sidebar file browser (text-file
    listing, folder navigation, single-buffer open). Sidebar extends the
    window outward. Files menu trimmed (no Statistics/Print).

 -- $DEBFULLNAME <$DEBEMAIL>  $(date -R)

EOF
      cat debian/changelog >> /tmp/newch
      mv /tmp/newch debian/changelog
    }

# debhelper config files must not be executable unless real scripts
find debian -maxdepth 1 -type f ! -name rules -exec chmod -x {} + 2>/dev/null || true

# --- build (unsigned binary-only package) ---
dpkg-buildpackage -us -uc -b 2>&1 | tail -25

# --- locate and rename the produced .deb ---
DEB=$(ls -t "$WORK"/*.deb 2>/dev/null | head -1)
if [ -z "$DEB" ]; then
  echo "ERROR: no .deb produced" >&2
  exit 1
fi
cp "$DEB" "$OUT"
echo ""
echo "=== DONE ==="
echo "Built: $DEB"
echo "Saved: $OUT"
ls -la "$OUT"
