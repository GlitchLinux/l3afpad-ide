/*
 *  L3afpad - GTK+ based simple text editor
 *  Settings module - persists user settings to
 *  $HOME/.config/l3afpad/l3afpad.json across launches.
 *
 *  Released under GPL-2.0, same as l3afpad.
 */

#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <gtk/gtk.h>

typedef struct {
	gint      width;
	gint      height;
	gchar    *fontname;
	gboolean  wordwrap;
	gboolean  linenumbers;
	gboolean  autoindent;
	gint      tabwidth;
	gchar    *theme;          /* selected color theme name, or NULL */
	gchar    *sidebar_dir;    /* last browsed folder, or NULL       */
	gboolean  sidebar_visible;
} Settings;

/* Load settings from the JSON file into *s, filling defaults for anything
 * missing or if the file does not exist. */
void settings_load(Settings *s);

/* Write *s to $HOME/.config/l3afpad/l3afpad.json (creating the dir). */
void settings_save(const Settings *s);

/* Free owned strings in *s. */
void settings_clear(Settings *s);

#endif /* _SETTINGS_H */
