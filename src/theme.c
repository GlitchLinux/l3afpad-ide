/*
 *  L3afpad - GTK+ based simple text editor
 *  Color theme module (rainglow base-color presets)
 *
 *  Released under GPL-2.0, same as l3afpad.
 */

#include "l3afpad.h"
#include "theme.h"
#include <string.h>

#ifndef PKGDATADIR
#define PKGDATADIR "/usr/local/share/l3afpad"
#endif

typedef struct {
	gchar *name;
	gchar *group;   /* Dark | Light | Contrast */
	gchar *bg, *fg, *sel, *caret, *linehl;
} Theme;

static GArray      *themes       = NULL;  /* array of Theme */
static GtkWidget   *theme_editor = NULL;  /* the GtkTextView */
static GtkCssProvider *provider  = NULL;
static gchar       *cur_name     = NULL;

/* ------------------------------------------------------------------ */
/* Load the table                                                     */
/* ------------------------------------------------------------------ */
static gchar *find_table(void)
{
	/* Search order: env override, install dir, alongside the binary,
	 * then the source tree (so it works before `make install`). */
	const gchar *env = g_getenv("L3AFPAD_THEMES");
	const gchar *candidates[] = {
		env,
		PKGDATADIR "/themes.tsv",
		"/usr/share/l3afpad/themes.tsv",
		"/usr/local/share/l3afpad/themes.tsv",
		"data/themes.tsv",
		"../data/themes.tsv",
		NULL
	};
	gint i;
	for (i = 0; candidates[i]; i++)
		if (candidates[i] && g_file_test(candidates[i], G_FILE_TEST_IS_REGULAR))
			return g_strdup(candidates[i]);
	return NULL;
}

gint theme_load_table(void)
{
	gchar *path, *contents = NULL;
	gsize len;
	gchar **lines;
	gint i, n = 0;

	if (themes)
		return themes->len;

	themes = g_array_new(FALSE, FALSE, sizeof(Theme));

	path = find_table();
	if (!path)
		return 0;
	if (!g_file_get_contents(path, &contents, &len, NULL)) {
		g_free(path);
		return 0;
	}
	g_free(path);

	lines = g_strsplit(contents, "\n", -1);
	g_free(contents);
	for (i = 0; lines[i]; i++) {
		gchar **f;
		if (!*lines[i])
			continue;
		f = g_strsplit(lines[i], "|", 7);
		if (g_strv_length(f) == 7) {
			Theme t;
			t.name   = g_strdup(f[0]);
			t.group  = g_strdup(f[1]);
			t.bg     = g_strdup(f[2]);
			t.fg     = g_strdup(f[3]);
			t.sel    = g_strdup(f[4]);
			t.caret  = g_strdup(f[5]);
			t.linehl = g_strdup(f[6]);
			g_array_append_val(themes, t);
			n++;
		}
		g_strfreev(f);
	}
	g_strfreev(lines);
	return n;
}

/* ------------------------------------------------------------------ */
/* Apply colors via CSS                                               */
/* ------------------------------------------------------------------ */
static void apply_theme(const Theme *t)
{
	gchar *css;
	GtkStyleContext *ctx;

	if (!theme_editor)
		return;

	if (!provider)
		provider = gtk_css_provider_new();

	/* Target the text view and its text; keep it minimal so we don't
	 * fight the user's GTK theme beyond the editor area. */
	css = g_strdup_printf(
		"textview, textview text {"
		"  background-color: %s;"
		"  color: %s;"
		"  caret-color: %s;"
		"}"
		"textview text selection {"
		"  background-color: %s;"
		"  color: %s;"
		"}",
		t->bg, t->fg, t->caret, t->sel, t->fg);

	gtk_css_provider_load_from_data(provider, css, -1, NULL);
	g_free(css);

	ctx = gtk_widget_get_style_context(theme_editor);
	gtk_style_context_add_provider(ctx,
		GTK_STYLE_PROVIDER(provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_free(cur_name);
	cur_name = g_strdup(t->name);
}

gboolean theme_apply_by_name(const gchar *name)
{
	guint i;
	if (!themes || !name)
		return FALSE;
	for (i = 0; i < themes->len; i++) {
		Theme *t = &g_array_index(themes, Theme, i);
		if (strcmp(t->name, name) == 0) {
			apply_theme(t);
			return TRUE;
		}
	}
	return FALSE;
}

const gchar *theme_current(void)
{
	return cur_name;
}

/* ------------------------------------------------------------------ */
/* Menu                                                               */
/* ------------------------------------------------------------------ */
static void on_theme_item(GtkMenuItem *item, gpointer data)
{
	const Theme *t = data;
	apply_theme(t);
}

void theme_set_view(GtkWidget *editor_view)
{
	theme_editor = editor_view;
}

GtkWidget *theme_build_menu(GtkWidget *editor_view)
{
	GtkWidget *themes_item, *root_menu;
	GtkWidget *sub_dark, *sub_light, *sub_contrast;
	GtkWidget *mi_dark, *mi_light, *mi_contrast;
	guint i;

	if (editor_view)
		theme_editor = editor_view;

	themes_item = gtk_menu_item_new_with_mnemonic(_("_Themes"));
	root_menu   = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(themes_item), root_menu);

	sub_dark     = gtk_menu_new();
	sub_light    = gtk_menu_new();
	sub_contrast = gtk_menu_new();

	mi_dark     = gtk_menu_item_new_with_label(_("Dark"));
	mi_light    = gtk_menu_item_new_with_label(_("Light"));
	mi_contrast = gtk_menu_item_new_with_label(_("Contrast"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_dark),     sub_dark);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_light),    sub_light);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_contrast), sub_contrast);
	gtk_menu_shell_append(GTK_MENU_SHELL(root_menu), mi_dark);
	gtk_menu_shell_append(GTK_MENU_SHELL(root_menu), mi_light);
	gtk_menu_shell_append(GTK_MENU_SHELL(root_menu), mi_contrast);
	gtk_widget_show(mi_dark);
	gtk_widget_show(mi_light);
	gtk_widget_show(mi_contrast);

	for (i = 0; themes && i < themes->len; i++) {
		Theme *t = &g_array_index(themes, Theme, i);
		GtkWidget *it = gtk_menu_item_new_with_label(t->name);
		GtkWidget *target = sub_dark;
		if (strcmp(t->group, "Light") == 0)
			target = sub_light;
		else if (strcmp(t->group, "Contrast") == 0)
			target = sub_contrast;
		g_signal_connect(it, "activate",
			G_CALLBACK(on_theme_item), t);
		gtk_menu_shell_append(GTK_MENU_SHELL(target), it);
		gtk_widget_show(it);
	}

	gtk_widget_show_all(themes_item);
	return themes_item;
}
