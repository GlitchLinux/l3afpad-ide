# l3afpad-ide

<img width="1236" height="1091" alt="714649528-85073e96-9cf4-4971-8c05-6656ab13d672" src="https://github.com/user-attachments/assets/35c605d5-a3e0-4d1c-8733-ff4326c008e1" />

---

This is a fork of [l3afpad](https://github.com/stevenhoneyman/l3afpad)
(itself a GTK3 fork of [Leafpad](http://tarot.freeshell.org/leafpad/)) that
adds editor conveniences while keeping the minimal feel.

## Features added by this fork

- **Sidebar file browser** - pick a local folder and browse its text files in
  a collapsible left pane; click to open, navigate into subfolders. The
  sidebar extends the window outward instead of shrinking the editor.
- **Syntax highlighting** - the editor is built on GtkSourceView, with
  automatic language detection on open (169 languages) and bracket matching.
- **326 colour themes** - the [rainglow](https://github.com/rainglow) theme
  collection converted to GtkSourceView style schemes, grouped Dark / Light /
  Contrast, with **live preview on hover** and a **Default** entry to restore
  stock colours.
- **Persistent settings** - theme, font, window size, sidebar folder and
  visibility, and the usual toggles are saved to
  `~/.config/l3afpad/l3afpad.json` and restored on launch.
- **Trimmed menus** and a custom icon set.

Everything l3afpad already did - undo/redo, find/replace, encodings, line
numbers, printing - still works unchanged.

## Building

Requires GTK+3, GtkSourceView 4, json-glib, and the usual autotools +
intltool:

```sh
sudo apt-get install -y build-essential autoconf automake intltool pkg-config \
    libgtk-3-dev libgtksourceview-4-dev libjson-glib-dev

./autogen.sh && ./configure && make
./src/l3afpad          # run from the build tree
sudo make install      # or install system-wide
```

## Install from the Glitch Linux apt repo

```sh
# (repo already configured per glitchlinux.com/apt)
sudo apt-get update
sudo apt-get install l3afpad-remaster
```

The package installs as a drop-in `l3afpad` binary.

## Credits

- **Leafpad** - Tarot Osuji
- **l3afpad** (GTK3 fork) - Wen-Yen Chuang (caleb), Steven Honeyman
- **rainglow themes** - Dayle Rees, MIT licensed
- Fork, IDE features and packaging - GlitchLinux

## Licence

GPL-2.0, same as the upstream editor. See [COPYING](COPYING). The bundled
rainglow themes are MIT licensed by their author.
