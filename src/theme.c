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
static gchar       *cur_name     = NULL;  /* committed (persisted) theme   */
static gchar       *saved_name   = NULL;  /* theme active when menu opened */
static gboolean     committed_in_menu = FALSE;

/* ------------------------------------------------------------------ */
/* Load the table                                                     */
/* ------------------------------------------------------------------ */
static gchar *find_table(void)
{
	const gchar *env = g_getenv("L3AFPAD_THEMES");
	gchar *exe = NULL, *exedir = NULL, *p;
	gchar *cand;
	gint i;

	/* 1) explicit override */
	if (env && g_file_test(env, G_FILE_TEST_IS_REGULAR))
		return g_strdup(env);

	/* 2) relative to the executable, so cwd does not matter.
	 *    installed:  <exedir>/../share/l3afpad/themes.tsv
	 *    source:     <exedir>/../data/themes.tsv   (src/l3afpad -> ../data) */
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

	/* 3) fixed install locations */
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

	/* 4) last resort: cwd-relative */
	if (g_file_test("data/themes.tsv", G_FILE_TEST_IS_REGULAR))
		return g_strdup("data/themes.tsv");

	(void)p;
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
/* Find a theme by name (internal). */
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

/* Visually apply colors only (does not change cur_name). Pass NULL to
 * remove the provider and restore the default GTK colors. */
static void apply_colors(const Theme *t)
{
	gchar *css;
	GtkStyleContext *ctx;

	if (!theme_editor)
		return;

	ctx = gtk_widget_get_style_context(theme_editor);

	if (!t) {
		/* restore default: drop our provider */
		if (provider) {
			gtk_style_context_remove_provider(ctx,
				GTK_STYLE_PROVIDER(provider));
			g_object_unref(provider);
			provider = NULL;
		}
		return;
	}

	if (!provider) {
		provider = gtk_css_provider_new();
		gtk_style_context_add_provider(ctx,
			GTK_STYLE_PROVIDER(provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

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
}

/* Commit a theme: apply it and remember it as the persisted choice. */
static void apply_theme(const Theme *t)
{
	apply_colors(t);
	g_free(cur_name);
	cur_name = t ? g_strdup(t->name) : NULL;
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
/* Menu                                                               */
/* ------------------------------------------------------------------ */
/* Hover a theme item: preview its colors (not committed). */
static void on_theme_select(GtkMenuItem *item, gpointer data)
{
	const Theme *t = data;
	apply_colors(t);
}

/* Click a theme item: commit it as the chosen theme. */
static void on_theme_item(GtkMenuItem *item, gpointer data)
{
	const Theme *t = data;
	apply_theme(t);
	committed_in_menu = TRUE;
}

/* The "Default" item: clear any theme and restore default colors. */
static void on_theme_default_select(GtkMenuItem *item, gpointer data)
{
	apply_colors(NULL);          /* preview the default look */
}
static void on_theme_default(GtkMenuItem *item, gpointer data)
{
	apply_theme(NULL);           /* commit "no theme" */
	committed_in_menu = TRUE;
}

/* Menu opened: remember the committed theme so we can revert if the user
 * only browses (previews) without clicking. */
static void on_root_menu_show(GtkWidget *w, gpointer data)
{
	g_free(saved_name);
	saved_name = cur_name ? g_strdup(cur_name) : NULL;
	committed_in_menu = FALSE;
}

/* Menu closed: if nothing was committed, revert to the theme that was
 * active when the menu opened (undo the hover previews). */
static void on_root_menu_hide(GtkWidget *w, gpointer data)
{
	if (committed_in_menu)
		return;
	if (saved_name) {
		const Theme *t = theme_find(saved_name);
		apply_colors(t);         /* restore previewed-away theme */
	} else {
		apply_colors(NULL);      /* was default */
	}
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

	/* Default (no theme) entry at the very top */
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

	/* revert previews if the menu is closed without a click */
	g_signal_connect(root_menu, "show",
		G_CALLBACK(on_root_menu_show), NULL);
	g_signal_connect(root_menu, "hide",
		G_CALLBACK(on_root_menu_hide), NULL);

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
		g_signal_connect(it, "select",
			G_CALLBACK(on_theme_select), t);
		g_signal_connect(it, "activate",
			G_CALLBACK(on_theme_item), t);
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
