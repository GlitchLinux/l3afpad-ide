/*
 *  L3afpad - GTK+ based simple text editor
 *  Color theme module (rainglow base-color presets)
 *
 *  Loads a table of color themes (name|group|bg|fg|sel|caret|linehl) and
 *  applies a selected theme's base colors to the editor GtkTextView via a
 *  GtkCssProvider. Builds a "Themes" menu (Dark/Light/Contrast submenus).
 *
 *  Released under GPL-2.0, same as l3afpad.
 */

#ifndef _THEME_H
#define _THEME_H

#include <gtk/gtk.h>

/* Load themes from the data table (PKGDATADIR/themes.tsv, with a fallback
 * search). Safe to call once at startup; returns number of themes loaded. */
gint theme_load_table(void);

/* Build a "Themes" GtkMenuItem (with Dark/Light/Contrast submenus) ready to
 * append to the menubar. editor_view is the GtkTextView colors apply to. */
GtkWidget *theme_build_menu(GtkWidget *editor_view);

/* Apply a theme by name (as it appears in the table). Returns TRUE on hit. */
/* Set the editor view that themes apply to (call once after the view
 * exists). */
void theme_set_view(GtkWidget *editor_view);

gboolean theme_apply_by_name(const gchar *name);

/* Name of the currently applied theme, or NULL. */
const gchar *theme_current(void);

#endif /* _THEME_H */
