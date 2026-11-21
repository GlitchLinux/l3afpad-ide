/*
 *  L3afpad - GTK+ based simple text editor
 *  Sidebar file browser module
 *
 *  Adds a collapsible left sidebar that lists files in a chosen local
 *  directory. Clicking a file opens it in the editor (single-buffer swap,
 *  reusing the existing modified-check and file_open_real path).
 *
 *  Released under GPL-2.0, same as l3afpad.
 */

#ifndef _SIDEBAR_H
#define _SIDEBAR_H

#include <gtk/gtk.h>

/* Build the sidebar widget (header buttons + file list).
 * Returns the top-level sidebar container to pack into the GtkPaned.
 * editor_view is the main GtkTextView the sidebar opens files into. */
GtkWidget *sidebar_create(GtkWidget *editor_view);

/* Toggle sidebar visibility (wired to the File/Files menu). */
void sidebar_toggle(void);

/* window.c registers the GtkPaned the sidebar lives in. */
void sidebar_set_paned(GtkWidget *paned);

/* Returns TRUE if the sidebar is currently visible. */
gboolean sidebar_is_visible(void);

/* Set the browse root and (re)populate the flat file list. */
void sidebar_set_root(const gchar *path);

/* Current browse root, or NULL. */
const gchar *sidebar_get_root(void);

#endif /* _SIDEBAR_H */
