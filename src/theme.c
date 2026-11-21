/*
 *  L3afpad - GTK+ based simple text editor
 *  Color theme module (rainglow themes as GtkSourceView style schemes)
 *
 *  Applies real syntax-highlighting color schemes to the GtkSourceBuffer.
 *  Builds a "Themes" menu (Dark/Light/Contrast submenus) with live preview
 *  on hover, a Default (no scheme) entry, and commit on click.
 *
 *  Released under GPL-2.0, same as l3afpad.
 */

#include "l3afpad.h"
#include "theme.h"
#include <gtksourceview/gtksource.h>
#include <string.h>

#ifndef PKGDATADIR
#define PKGDATADIR "/usr/local/share/l3afpad"
#endif

typedef struct {
	gchar *slug;    /* scheme slug; scheme id is "rainglow-<slug>" */
	gchar *name;    /* display name */
	gchar *group;   /* Dark | Light | Contrast */
} Theme;

static GArray    *themes       = NULL;
static GtkWidget *theme_editor = NULL;   /* the GtkSourceView */
static gchar     *cur_name     = NULL;   /* committed theme name, or NULL */
static gchar     *saved_name   = NULL;   /* theme active when menu opened  */
static gboolean   committed_in_menu = FALSE;
static GtkSourceStyleSchemeManager *scheme_mgr = NULL;

/* ------------------------------------------------------------------ */
/* Locate the schemes directory (relative to the executable).         */
/* ------------------------------------------------------------------ */
static gchar *find_schemes_dir(void)
{
	const gchar *env = g_getenv("L3AFPAD_SCHEMES");
	gchar *exe, *exedir, *cand;
	gint i;

	if (env && g_file_test(env, G_FILE_TEST_IS_DIR))
		return g_strdup(env);

	exe = g_file_read_link("/proc/self/exe", NULL);
	if (exe) {
		exedir = g_path_get_dirname(exe);
		const gchar *rel[] = {
			"../share/l3afpad/schemes",
			"../data/schemes",
			"data/schemes",
			NULL
		};
		for (i = 0; rel[i]; i++) {
			cand = g_build_filename(exedir, rel[i], NULL);
			if (g_file_test(cand, G_FILE_TEST_IS_DIR)) {
				g_free(exe); g_free(exedir);
				return cand;
			}
			g_free(cand);
		}
		g_free(exe); g_free(exedir);
	}

	{
		const gchar *fixed[] = {
			PKGDATADIR "/schemes",
			"/usr/share/l3afpad/schemes",
			"/usr/local/share/l3afpad/schemes",
			NULL
		};
		for (i = 0; fixed[i]; i++)
			if (g_file_test(fixed[i], G_FILE_TEST_IS_DIR))
				return g_strdup(fixed[i]);
	}
	return NULL;
}

/* Locate themes.tsv (same strategy, file). */
static gchar *find_table(void)
{
	const gchar *env = g_getenv("L3AFPAD_THEMES");
	gchar *exe, *exedir, *cand;
	gint i;

	if (env && g_file_test(env, G_FILE_TEST_IS_REGULAR))
		return g_strdup(env);

	exe = g_file_read_link("/proc/self/exe", NULL);
	if (exe) {
		exedir = g_path_get_dirname(exe);
		const gchar *rel[] = {
			"../share/l3afpad/themes.tsv",
			"../data/themes.tsv",
			"data/themes.tsv",
			NULL
		};
		for (i = 0; rel[i]; i++) {
			cand = g_build_filename(exedir, rel[i], NULL);
			if (g_file_test(cand, G_FILE_TEST_IS_REGULAR)) {
				g_free(exe); g_free(exedir);
				return cand;
			}
			g_free(cand);
		}
		g_free(exe); g_free(exedir);
	}
	{
		const gchar *fixed[] = {
			PKGDATADIR "/themes.tsv",
			"/usr/share/l3afpad/themes.tsv",
			"/usr/local/share/l3afpad/themes.tsv",
			NULL
		};
		for (i = 0; fixed[i]; i++)
			if (g_file_test(fixed[i], G_FILE_TEST_IS_REGULAR))
				return g_strdup(fixed[i]);
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/* Load the table + register the scheme search path                   */
/* ------------------------------------------------------------------ */
gint theme_load_table(void)
{
	gchar *path, *contents = NULL, *schemes_dir;
	gchar **lines;
	gint i, n = 0;

	if (themes)
		return themes->len;

	themes = g_array_new(FALSE, FALSE, sizeof(Theme));

	/* register schemes dir with GtkSourceView */
	scheme_mgr = gtk_source_style_scheme_manager_new();
	schemes_dir = find_schemes_dir();
	if (schemes_dir) {
		const gchar *dirs[] = { schemes_dir, NULL };
		gtk_source_style_scheme_manager_set_search_path(
			scheme_mgr, (gchar **) dirs);
		g_free(schemes_dir);
	}

	path = find_table();
	if (!path)
		return 0;
	if (!g_file_get_contents(path, &contents, NULL, NULL)) {
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
		f = g_strsplit(lines[i], "|", 3);
		if (g_strv_length(f) == 3) {
			Theme t;
			t.slug  = g_strdup(f[0]);
			t.name  = g_strdup(f[1]);
			t.group = g_strdup(f[2]);
			g_array_append_val(themes, t);
			n++;
		}
		g_strfreev(f);
	}
	g_strfreev(lines);
	return n;
}

/* ------------------------------------------------------------------ */
/* Apply a scheme                                                     */
/* ------------------------------------------------------------------ */
static const Theme *theme_find(const gchar *name)
{
	guint i;
	if (!themes || !name)
		return NULL;
	for (i = 0; i < themes->len; i++) {
		Theme *t = &g_array_index(themes, Theme, i);
		if (strcmp(t->name, name) == 0)
			return t;
	}
	return NULL;
}

/* Visually apply a theme's scheme (does not change cur_name).
 * Pass NULL to clear the scheme (default look). */
static void apply_scheme(const Theme *t)
{
	GtkSourceBuffer *buf;
	GtkSourceStyleScheme *scheme = NULL;

	if (!theme_editor || !GTK_SOURCE_IS_VIEW(theme_editor))
		return;
	buf = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
		GTK_TEXT_VIEW(theme_editor)));

	if (t && scheme_mgr) {
		gchar *id = g_strdup_printf("rainglow-%s", t->slug);
		scheme = gtk_source_style_scheme_manager_get_scheme(scheme_mgr, id);
		g_free(id);
	}
	gtk_source_buffer_set_style_scheme(buf, scheme);
}

/* Commit a theme. */
static void apply_theme(const Theme *t)
{
	apply_scheme(t);
	g_free(cur_name);
	cur_name = t ? g_strdup(t->name) : NULL;
}

void theme_set_view(GtkWidget *editor_view)
{
	theme_editor = editor_view;
}

gboolean theme_apply_by_name(const gchar *name)
{
	const Theme *t = theme_find(name);
	if (!t)
		return FALSE;
	apply_theme(t);
	return TRUE;
}

const gchar *theme_current(void)
{
	return cur_name;
}

/* ------------------------------------------------------------------ */
/* Menu handlers                                                      */
/* ------------------------------------------------------------------ */
static void on_theme_select(GtkMenuItem *item, gpointer data)
{
	apply_scheme((const Theme *) data);   /* preview */
}
static void on_theme_item(GtkMenuItem *item, gpointer data)
{
	apply_theme((const Theme *) data);    /* commit  */
	committed_in_menu = TRUE;
}
static void on_theme_default_select(GtkMenuItem *item, gpointer data)
{
	apply_scheme(NULL);
}
static void on_theme_default(GtkMenuItem *item, gpointer data)
{
	apply_theme(NULL);
	committed_in_menu = TRUE;
}
static void on_root_menu_show(GtkWidget *w, gpointer data)
{
	g_free(saved_name);
	saved_name = cur_name ? g_strdup(cur_name) : NULL;
	committed_in_menu = FALSE;
}
static void on_root_menu_hide(GtkWidget *w, gpointer data)
{
	if (committed_in_menu)
		return;
	if (saved_name)
		apply_scheme(theme_find(saved_name));
	else
		apply_scheme(NULL);
}

/* ------------------------------------------------------------------ */
/* Build the menu                                                     */
/* ------------------------------------------------------------------ */
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

	/* Default (no scheme) at the top */
	{
		GtkWidget *mi_default = gtk_menu_item_new_with_label(_("Default"));
		g_signal_connect(mi_default, "select",
			G_CALLBACK(on_theme_default_select), NULL);
		g_signal_connect(mi_default, "activate",
			G_CALLBACK(on_theme_default), NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(root_menu), mi_default);
		gtk_widget_show(mi_default);
		GtkWidget *sep = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(root_menu), sep);
		gtk_widget_show(sep);
	}

	g_signal_connect(root_menu, "show", G_CALLBACK(on_root_menu_show), NULL);
	g_signal_connect(root_menu, "hide", G_CALLBACK(on_root_menu_hide), NULL);

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
		g_signal_connect(it, "select", G_CALLBACK(on_theme_select), t);
		g_signal_connect(it, "activate", G_CALLBACK(on_theme_item), t);
		gtk_menu_shell_append(GTK_MENU_SHELL(target), it);
		gtk_widget_show(it);
	}

	gtk_widget_show_all(sub_dark);
	gtk_widget_show_all(sub_light);
	gtk_widget_show_all(sub_contrast);
	gtk_widget_show_all(root_menu);
	gtk_widget_show_all(themes_item);
	return themes_item;
}
