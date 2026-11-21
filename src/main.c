/*
 *  L3afpad - GTK+ based simple text editor
 *  Copyright (C) 2004-2005 Tarot Osuji
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _L3AFPAD_MAIN
#include "l3afpad.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PublicData *pub;
Settings app_settings;

/* Settings are persisted as JSON in ~/.config/l3afpad/l3afpad.json
 * via the settings module. save_config_file() keeps its name because it
 * is called from the quit path and elsewhere. */

static gboolean opt_get(GtkUIManager *ui, const gchar *path)
{
	return gtk_toggle_action_get_active(
		GTK_TOGGLE_ACTION(gtk_ui_manager_get_action(ui, path)));
}

void save_config_file(void)
{
	Settings s;
	gchar *fontname;

	memset(&s, 0, sizeof(s));

	gtk_window_get_size(GTK_WINDOW(pub->mw->window), &s.width, &s.height);
	fontname = pango_font_description_to_string(
		gtk_style_context_get_font(
			gtk_widget_get_style_context(pub->mw->view), 0));
	s.fontname        = fontname;
	s.wordwrap        = opt_get(pub->mw->menubar, "/M/Options/WordWrap");
	s.linenumbers     = opt_get(pub->mw->menubar, "/M/Options/LineNumbers");
	s.autoindent      = opt_get(pub->mw->menubar, "/M/Options/AutoIndent");
	s.tabwidth        = get_current_tab_width();
	s.theme           = (gchar *) theme_current();
	s.sidebar_dir     = (gchar *) sidebar_get_root();
	s.sidebar_visible = sidebar_is_visible();

	settings_save(&s);

	g_free(fontname);
}

gint jump_linenum = 0;
GtkClipboard *selection_primary = NULL;

static void parse_args(gint argc, gchar **argv, FileInfo *fi)
{
	EncArray *encarray;
	gint i;
	GError *error = NULL;

	GOptionContext *context;
	gchar *opt_codeset = NULL;
	gint opt_tab_width = 0;
	gboolean opt_jump = 0;
	gboolean opt_version = FALSE;
	GOptionEntry entries[] =
	{
		{ "codeset", 0, 0, G_OPTION_ARG_STRING, &opt_codeset, "Set codeset to open file", "CODESET" },
		{ "tab-width", 0, 0, G_OPTION_ARG_INT, &opt_tab_width, "Set tab width", "WIDTH" },
		{ "jump", 0, 0, G_OPTION_ARG_INT, &opt_jump, "Jump to specified line", "LINENUM" },
		{ "version", 0, 0, G_OPTION_ARG_NONE, &opt_version, "Show version number", NULL },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	context = g_option_context_new("[filename]");
	g_option_context_add_main_entries(context, entries, PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_set_ignore_unknown_options(context, FALSE);
	g_option_context_parse(context, &argc, &argv, &error);
	g_option_context_free(context);

	if (error) {
		g_print("%s: %s\n", PACKAGE, error->message);
		g_error_free(error);
		exit(-1);
	}
	if (opt_version) {
		g_print("%s\n", PACKAGE_STRING);
		exit(0);
	}
	if (opt_codeset) {
		g_convert("TEST", -1, "UTF-8", opt_codeset, NULL, NULL, &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		} else {
			g_free(fi->charset);
			fi->charset = g_strdup(opt_codeset);
		}
	}
	if (opt_tab_width)
		indent_set_default_tab_width(opt_tab_width);
	if (opt_jump)
		jump_linenum = opt_jump;

	if (fi->charset
		&& (g_ascii_strcasecmp(fi->charset, get_default_charset()) != 0)
		&& (g_ascii_strcasecmp(fi->charset, "UTF-8") != 0)) {
		encarray = get_encoding_items(get_encoding_code());
		for (i = 0; i < ENCODING_MAX_ITEM_NUM; i++)
			if (encarray->item[i])
				if (g_ascii_strcasecmp(fi->charset, encarray->item[i]) == 0)
					break;
		if (i == ENCODING_MAX_ITEM_NUM)
			fi->charset_flag = TRUE;
	}

	if (argc >= 2)
		fi->filename = parse_file_uri(argv[1]);
}

gint main(gint argc, gchar **argv)
{
	gchar *stdin_data = NULL;

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	pub = g_malloc(sizeof(PublicData));
	pub->fi = g_malloc(sizeof(FileInfo));
	pub->fi->filename     = NULL;
	pub->fi->charset      = NULL;
	pub->fi->charset_flag = FALSE;
	pub->fi->lineend      = LF;

	parse_args(argc, argv, pub->fi);

#if !ENABLE_XINPUT2
	gdk_disable_multidevice();
#endif

	gtk_init(&argc, &argv);
	g_set_application_name(PACKAGE_NAME);

	selection_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);

	pub->mw = create_main_window();

	settings_load(&app_settings);

	gtk_window_set_default_size(
		GTK_WINDOW(pub->mw->window), app_settings.width, app_settings.height);
	set_text_font_by_name(pub->mw->view, app_settings.fontname);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		gtk_ui_manager_get_widget(pub->mw->menubar, "/M/Options/WordWrap")),
		app_settings.wordwrap);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		gtk_ui_manager_get_widget(pub->mw->menubar, "/M/Options/LineNumbers")),
		app_settings.linenumbers);
	indent_set_default_tab_width(app_settings.tabwidth);
	indent_refresh_tab_width(pub->mw->view);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		gtk_ui_manager_get_widget(pub->mw->menubar, "/M/Options/AutoIndent")),
		app_settings.autoindent);

	/* restore theme */
	if (app_settings.theme && *app_settings.theme)
		theme_apply_by_name(app_settings.theme);

	/* restore sidebar folder + visibility */
	if (app_settings.sidebar_dir && *app_settings.sidebar_dir)
		sidebar_set_root(app_settings.sidebar_dir);
	if (app_settings.sidebar_visible)
		sidebar_toggle();

	gtk_widget_show_all(pub->mw->window);

#if ENABLE_EMACS
	check_emacs_key_theme(GTK_WINDOW(pub->mw->window), pub->mw->menubar);
#endif

	hlight_init(pub->mw->buffer);
	undo_init(pub->mw->view,
		gtk_ui_manager_get_widget(pub->mw->menubar, "/M/Edit/Undo"),
		gtk_ui_manager_get_widget(pub->mw->menubar, "/M/Edit/Redo"));
//	hlight_init(pub->mw->buffer);
	dnd_init(pub->mw->view);

	if (pub->fi->filename)
		file_open_real(pub->mw->view, pub->fi);
#ifdef G_OS_UNIX
	else
		stdin_data = gedit_utils_get_stdin();
#endif
	if (stdin_data) {
		gchar *str;
		GtkTextIter iter;

		str = g_convert(stdin_data, -1, "UTF-8",
			get_default_charset(), NULL, NULL, NULL);
		g_free(stdin_data);

//		gtk_text_buffer_set_text(buffer, "", 0);
		gtk_text_buffer_get_start_iter(pub->mw->buffer, &iter);
		gtk_text_buffer_insert(pub->mw->buffer, &iter, str, strlen(str));
		gtk_text_buffer_get_start_iter(pub->mw->buffer, &iter);
		gtk_text_buffer_place_cursor(pub->mw->buffer, &iter);
		gtk_text_buffer_set_modified(pub->mw->buffer, FALSE);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(pub->mw->view), &iter, 0, FALSE, 0, 0);
		g_free(str);
	}

	if (jump_linenum) {
		GtkTextIter iter;

		gtk_text_buffer_get_iter_at_line(pub->mw->buffer, &iter, jump_linenum - 1);
		gtk_text_buffer_place_cursor(pub->mw->buffer, &iter);
//		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview), &iter, 0.1, FALSE, 0.5, 0.5);
		scroll_to_cursor(pub->mw->buffer, 0.25);
	}

	set_main_window_title();
	indent_refresh_tab_width(pub->mw->view);
//	hlight_apply_all(pub->mw->buffer);

	gtk_main();

	return 0;
}
