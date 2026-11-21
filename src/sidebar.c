/*
 *  L3afpad - GTK+ based simple text editor
 *  Sidebar file browser module
 *
 *  Adds a collapsible left sidebar listing files in a chosen directory.
 *  Clicking a file opens it in the editor using the same modified-check
 *  and file_open_real path as File -> Open, so behaviour matches the
 *  existing editor exactly (single-buffer swap).
 *
 *  Released under GPL-2.0, same as l3afpad.
 */

#include "l3afpad.h"
#include "sidebar.h"
#include <string.h>

/* Columns in the list store. */
enum {
	COL_ICON,      /* icon name (string)   */
	COL_NAME,      /* display name         */
	COL_PATH,      /* absolute path        */
	COL_IS_DIR,    /* gboolean             */
	N_COLS
};

/* Fixed sidebar width. Toggling grows/shrinks the window by this much
 * (plus the paned handle) so the editor keeps its size and the sidebar
 * extends outward to the left instead of eating into the editor. */
#define SIDEBAR_WIDTH 220

/* Module-global widgets (single-window app, mirrors colors.c style). */
static GtkWidget   *sidebar_box   = NULL;  /* top container in the paned */
static GtkWidget   *sidebar_paned = NULL;  /* the GtkPaned we live in    */
static GtkWidget   *tree_view     = NULL;
static GtkListStore *store        = NULL;
static GtkWidget   *root_label    = NULL;
static GtkWidget   *editor        = NULL;  /* main text view */
static gchar       *current_root  = NULL;

/* ------------------------------------------------------------------ */
/* Opening a file: replicate the in-place open branch of on_file_open  */
/* (callback.c, non-CSDI): check modified -> file_open_real -> swap fi */
/* ------------------------------------------------------------------ */
static void sidebar_open_file(const gchar *filename)
{
	FileInfo *fi;

	if (!filename)
		return;

	/* Shows the "save changes?" prompt; non-zero means cancel. */
	if (check_text_modification())
		return;

	fi = g_malloc(sizeof(FileInfo));
	fi->filename     = g_strdup(filename);
	fi->charset      = NULL;
	fi->charset_flag = FALSE;
	fi->lineend      = LF;

	if (file_open_real(pub->mw->view, fi)) {
		/* open failed: free our throwaway fi */
		g_free(fi->filename);
		g_free(fi);
	} else {
		g_free(pub->fi->filename);
		g_free(pub->fi->charset);
		g_free(pub->fi);
		pub->fi = fi;
		undo_clear_all(pub->mw->buffer);
		set_main_window_title();
		force_call_cb_modified_changed(pub->mw->view);
	}
}

/* ------------------------------------------------------------------ */
/* Populate the flat list for current_root                            */
/* ------------------------------------------------------------------ */
static gint cmp_entries(gconstpointer a, gconstpointer b)
{
	/* dirs first, then case-insensitive name */
	const gchar *na = a;
	const gchar *nb = b;
	return g_ascii_strcasecmp(na, nb);
}

/* Content sniff: read up to the first 4 KiB and decide whether the file
 * looks like text. Heuristic (same spirit as git/file):
 *   - empty file            -> text
 *   - contains a NUL byte    -> binary
 *   - valid UTF-8            -> text
 *   - else: mostly printable -> text
 * Avoids listing binaries in the sidebar without needing an extension list. */
static gboolean is_text_file(const gchar *path)
{
	FILE *fp;
	unsigned char buf[4096];
	size_t n, i;
	size_t ctrl = 0;

	fp = fopen(path, "rb");
	if (!fp)
		return FALSE;
	n = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);

	if (n == 0)
		return TRUE;                 /* empty = text */

	for (i = 0; i < n; i++)
		if (buf[i] == 0)
			return FALSE;        /* NUL => binary */

	/* valid UTF-8 across the sampled chunk? (may clip a trailing multibyte
	 * char at the 4 KiB boundary, which g_utf8_validate reports via end) */
	if (g_utf8_validate((const gchar *)buf, n, NULL))
		return TRUE;

	/* fallback: count non-printable, non-whitespace control bytes */
	for (i = 0; i < n; i++) {
		unsigned char c = buf[i];
		if (c < 0x09 || (c > 0x0d && c < 0x20))
			ctrl++;
	}
	return (ctrl * 100 / n) < 10;        /* <10% control bytes => text */
}

static void sidebar_populate(void)
{
	GDir *dir;
	const gchar *name;
	GError *err = NULL;
	GList *dirs = NULL, *files = NULL, *l;
	GtkTreeIter iter;

	gtk_list_store_clear(store);

	if (!current_root)
		return;

	dir = g_dir_open(current_root, 0, &err);
	if (!dir) {
		if (err)
			g_error_free(err);
		return;
	}

	/* Parent entry (".." ) so user can walk up without a tree. */
	{
		gchar *parent = g_path_get_dirname(current_root);
		if (parent && strcmp(parent, current_root) != 0) {
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
				COL_ICON, "go-up-symbolic",
				COL_NAME, "..",
				COL_PATH, parent,
				COL_IS_DIR, TRUE,
				-1);
		}
		g_free(parent);
	}

	while ((name = g_dir_read_name(dir))) {
		gchar *full = g_build_filename(current_root, name, NULL);
		if (g_file_test(full, G_FILE_TEST_IS_DIR))
			dirs = g_list_prepend(dirs, g_strdup(name));
		else if (is_text_file(full))
			files = g_list_prepend(files, g_strdup(name));
		/* non-text files are skipped */
		g_free(full);
	}
	g_dir_close(dir);

	dirs  = g_list_sort(dirs,  cmp_entries);
	files = g_list_sort(files, cmp_entries);

	for (l = dirs; l; l = l->next) {
		gchar *full = g_build_filename(current_root, l->data, NULL);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			COL_ICON, "folder-symbolic",
			COL_NAME, (gchar *)l->data,
			COL_PATH, full,
			COL_IS_DIR, TRUE,
			-1);
		g_free(full);
		g_free(l->data);
	}
	for (l = files; l; l = l->next) {
		gchar *full = g_build_filename(current_root, l->data, NULL);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			COL_ICON, "text-x-generic-symbolic",
			COL_NAME, (gchar *)l->data,
			COL_PATH, full,
			COL_IS_DIR, FALSE,
			-1);
		g_free(full);
		g_free(l->data);
	}
	g_list_free(dirs);
	g_list_free(files);
}

void sidebar_set_root(const gchar *path)
{
	if (!path)
		return;
	g_free(current_root);
	current_root = g_strdup(path);
	if (root_label)
		gtk_label_set_text(GTK_LABEL(root_label), current_root);
	sidebar_populate();
}

/* ------------------------------------------------------------------ */
/* Signal handlers                                                    */
/* ------------------------------------------------------------------ */
static void on_row_activated(GtkTreeView *tv, GtkTreePath *path,
	GtkTreeViewColumn *col, gpointer data)
{
	GtkTreeIter iter;
	gchar *fullpath = NULL;
	gboolean is_dir = FALSE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
		COL_PATH, &fullpath, COL_IS_DIR, &is_dir, -1);

	if (fullpath) {
		if (is_dir)
			sidebar_set_root(fullpath);   /* navigate into/up */
		else
			sidebar_open_file(fullpath);  /* open in editor   */
		g_free(fullpath);
	}
}

static void on_choose_folder(GtkButton *btn, gpointer data)
{
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new(_("Choose Folder"),
		GTK_WINDOW(pub->mw->window),
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"), GTK_RESPONSE_OK,
		NULL);
	if (current_root)
		gtk_file_chooser_set_current_folder(
			GTK_FILE_CHOOSER(dialog), current_root);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		gchar *folder = gtk_file_chooser_get_filename(
			GTK_FILE_CHOOSER(dialog));
		if (folder) {
			sidebar_set_root(folder);
			g_free(folder);
		}
	}
	gtk_widget_destroy(dialog);
}

/* Thin wrappers so the header buttons reuse the existing menu callbacks. */
static void on_btn_new(GtkButton *b, gpointer d)     { on_file_new(); }
static void on_btn_open(GtkButton *b, gpointer d)     { on_file_open(); }
static void on_btn_save(GtkButton *b, gpointer d)     { on_file_save(); }
static void on_btn_saveas(GtkButton *b, gpointer d)   { on_file_save_as(); }
static void on_btn_refresh(GtkButton *b, gpointer d)  { sidebar_populate(); }

/* ------------------------------------------------------------------ */
/* Construction                                                       */
/* ------------------------------------------------------------------ */
/* label is a short text fallback shown when the icon theme lacks icon_name
 * (e.g. a minimal environment with no symbolic icons installed). */
static GtkWidget *make_icon_button2(const gchar *icon_name, const gchar *label,
	const gchar *tooltip, GCallback cb)
{
	GtkWidget *btn;
	GtkIconTheme *theme = gtk_icon_theme_get_default();

	if (theme && gtk_icon_theme_has_icon(theme, icon_name)) {
		btn = gtk_button_new_from_icon_name(icon_name,
			GTK_ICON_SIZE_SMALL_TOOLBAR);
	} else {
		/* no such icon -> use text so the control is still visible */
		btn = gtk_button_new_with_label(label);
	}
	gtk_widget_set_tooltip_text(btn, tooltip);
	gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
	g_signal_connect(btn, "clicked", cb, NULL);
	return btn;
}

static GtkWidget *make_icon_button(const gchar *icon_name,
	const gchar *tooltip, GCallback cb)
{
	return make_icon_button2(icon_name, tooltip, tooltip, cb);
}

GtkWidget *sidebar_create(GtkWidget *editor_view)
{
	GtkWidget *header;
	GtkWidget *folder_row;
	GtkWidget *sw;
	GtkCellRenderer *r_icon, *r_text;
	GtkTreeViewColumn *column;

	editor = editor_view;

	sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request(sidebar_box, SIDEBAR_WIDTH, -1);

	/* --- header row: file actions (reuse existing callbacks) --- */
	header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(header),
		make_icon_button("document-new-symbolic", _("New"),
			G_CALLBACK(on_btn_new)), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(header),
		make_icon_button("document-open-symbolic", _("Open..."),
			G_CALLBACK(on_btn_open)), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(header),
		make_icon_button("document-save-symbolic", _("Save"),
			G_CALLBACK(on_btn_save)), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(header),
		make_icon_button("document-save-as-symbolic", _("Save As..."),
			G_CALLBACK(on_btn_saveas)), FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(header),
		make_icon_button("view-refresh-symbolic", _("Refresh"),
			G_CALLBACK(on_btn_refresh)), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar_box), header, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(sidebar_box),
		gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

	/* --- folder chooser row --- */
	folder_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(folder_row),
		make_icon_button("folder-open-symbolic", _("Browse a folder"),
			G_CALLBACK(on_choose_folder)), FALSE, FALSE, 0);
	root_label = gtk_label_new(_("(no folder)"));
	gtk_label_set_ellipsize(GTK_LABEL(root_label), PANGO_ELLIPSIZE_START);
	gtk_label_set_xalign(GTK_LABEL(root_label), 0.0);
	gtk_box_pack_start(GTK_BOX(folder_row), root_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(sidebar_box), folder_row, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(sidebar_box),
		gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

	/* --- file list --- */
	store = gtk_list_store_new(N_COLS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

	column = gtk_tree_view_column_new();
	r_icon = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, r_icon, FALSE);
	gtk_tree_view_column_set_attributes(column, r_icon,
		"icon-name", COL_ICON, NULL);
	r_text = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, r_text, TRUE);
	gtk_tree_view_column_set_attributes(column, r_text,
		"text", COL_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	g_signal_connect(tree_view, "row-activated",
		G_CALLBACK(on_row_activated), NULL);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(sw), tree_view);
	gtk_widget_set_vexpand(sw, TRUE);
	gtk_box_pack_start(GTK_BOX(sidebar_box), sw, TRUE, TRUE, 0);

	/* Show all children now so they are ready; keep the box itself hidden
	 * until toggled. We do NOT use no_show_all (it would stop show_all from
	 * recursing into the children and leave an empty panel). Instead the
	 * window's startup show_all is run before we hide the box: see below.
	 * To guarantee the box starts hidden regardless of startup order, the
	 * toggle shows/hides only the box; children are already visible. */
	/* Show the children now, then hide only the top box. The window's
	 * startup show_all must NOT re-reveal the box, so mark it no_show_all;
	 * the toggle clears that flag before calling show_all so the children
	 * (already shown) become visible together with the box. */
	gtk_widget_show_all(sidebar_box);
	gtk_widget_set_no_show_all(sidebar_box, TRUE);
	gtk_widget_hide(sidebar_box);

	return sidebar_box;
}

/* Called by window.c after it builds the GtkPaned. */
void sidebar_set_paned(GtkWidget *paned)
{
	sidebar_paned = paned;
}

/* Width taken by the sidebar including the paned divider handle. */
static gint sidebar_total_width(void)
{
	gint handle = 0;
	if (sidebar_paned)
		gtk_widget_style_get(sidebar_paned,
			"handle-size", &handle, NULL);
	return SIDEBAR_WIDTH + handle;
}

void sidebar_toggle(void)
{
	GtkWidget *win;
	gint w, h, delta;

	if (!sidebar_box || !pub || !pub->mw)
		return;

	win   = pub->mw->window;
	delta = sidebar_total_width();
	gtk_window_get_size(GTK_WINDOW(win), &w, &h);

	if (gtk_widget_get_visible(sidebar_box)) {
		/* hide: shrink the window back by the sidebar width */
		gtk_widget_hide(sidebar_box);
		gtk_window_resize(GTK_WINDOW(win),
			(w - delta > 1) ? w - delta : 1, h);
	} else {
		/* show: grow the window so the editor keeps its width and the
		 * sidebar extends outward to the left. Children were already
		 * shown at creation, so just reveal the box. */
		gtk_widget_set_no_show_all(sidebar_box, FALSE);
		gtk_widget_show_all(sidebar_box);
		gtk_window_resize(GTK_WINDOW(win), w + delta, h);
		if (sidebar_paned)
			gtk_paned_set_position(GTK_PANED(sidebar_paned),
				SIDEBAR_WIDTH);
	}
}

gboolean sidebar_is_visible(void)
{
	return sidebar_box && gtk_widget_get_visible(sidebar_box);
}
