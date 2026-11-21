/*
 *  L3afpad - GTK+ based simple text editor
 *  Settings module (JSON persistence via json-glib)
 *
 *  Released under GPL-2.0, same as l3afpad.
 */

#include "l3afpad.h"
#include "settings.h"
#include <json-glib/json-glib.h>

static gchar *settings_path(void)
{
	return g_build_filename(g_get_user_config_dir(),
		"l3afpad", "l3afpad.json", NULL);
}

static void set_defaults(Settings *s)
{
	s->width           = 600;
	s->height          = 400;
	s->fontname        = g_strdup("Monospace 12");
	s->wordwrap        = FALSE;
	s->linenumbers     = FALSE;
	s->autoindent      = FALSE;
	s->tabwidth        = 8;
	s->theme           = NULL;
	s->sidebar_dir     = NULL;
	s->sidebar_visible = FALSE;
}

/* helpers to read a member with a fallback already in place */
static const gchar *obj_str(JsonObject *o, const gchar *k)
{
	if (json_object_has_member(o, k)) {
		JsonNode *n = json_object_get_member(o, k);
		if (JSON_NODE_HOLDS_VALUE(n))
			return json_object_get_string_member(o, k);
	}
	return NULL;
}
static gboolean obj_has_int(JsonObject *o, const gchar *k)
{
	return json_object_has_member(o, k)
	    && JSON_NODE_HOLDS_VALUE(json_object_get_member(o, k));
}

void settings_load(Settings *s)
{
	gchar *path;
	JsonParser *parser;
	JsonNode *root;
	JsonObject *o;
	const gchar *str;
	GError *err = NULL;

	set_defaults(s);

	path = settings_path();
	if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
		g_free(path);
		return;
	}

	parser = json_parser_new();
	if (!json_parser_load_from_file(parser, path, &err)) {
		if (err) g_error_free(err);
		g_object_unref(parser);
		g_free(path);
		return;
	}
	g_free(path);

	root = json_parser_get_root(parser);
	if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
		g_object_unref(parser);
		return;
	}
	o = json_node_get_object(root);

	if (obj_has_int(o, "width"))        s->width        = json_object_get_int_member(o, "width");
	if (obj_has_int(o, "height"))       s->height       = json_object_get_int_member(o, "height");
	if (obj_has_int(o, "tabwidth"))     s->tabwidth     = json_object_get_int_member(o, "tabwidth");
	if (json_object_has_member(o, "wordwrap"))    s->wordwrap    = json_object_get_boolean_member(o, "wordwrap");
	if (json_object_has_member(o, "linenumbers")) s->linenumbers = json_object_get_boolean_member(o, "linenumbers");
	if (json_object_has_member(o, "autoindent"))  s->autoindent  = json_object_get_boolean_member(o, "autoindent");
	if (json_object_has_member(o, "sidebar_visible")) s->sidebar_visible = json_object_get_boolean_member(o, "sidebar_visible");

	str = obj_str(o, "font");
	if (str) { g_free(s->fontname); s->fontname = g_strdup(str); }
	str = obj_str(o, "theme");
	if (str) { g_free(s->theme); s->theme = g_strdup(str); }
	str = obj_str(o, "sidebar_dir");
	if (str) { g_free(s->sidebar_dir); s->sidebar_dir = g_strdup(str); }

	g_object_unref(parser);
}

void settings_save(const Settings *s)
{
	gchar *dir, *path, *data;
	JsonBuilder *b;
	JsonGenerator *gen;
	JsonNode *root;
	GError *err = NULL;

	dir = g_build_filename(g_get_user_config_dir(), "l3afpad", NULL);
	if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
		g_mkdir_with_parents(dir, 0700);
	g_free(dir);

	b = json_builder_new();
	json_builder_begin_object(b);

	json_builder_set_member_name(b, "width");        json_builder_add_int_value(b, s->width);
	json_builder_set_member_name(b, "height");       json_builder_add_int_value(b, s->height);
	json_builder_set_member_name(b, "font");         json_builder_add_string_value(b, s->fontname ? s->fontname : "Monospace 12");
	json_builder_set_member_name(b, "wordwrap");     json_builder_add_boolean_value(b, s->wordwrap);
	json_builder_set_member_name(b, "linenumbers");  json_builder_add_boolean_value(b, s->linenumbers);
	json_builder_set_member_name(b, "autoindent");   json_builder_add_boolean_value(b, s->autoindent);
	json_builder_set_member_name(b, "tabwidth");     json_builder_add_int_value(b, s->tabwidth);
	json_builder_set_member_name(b, "theme");        json_builder_add_string_value(b, s->theme ? s->theme : "");
	json_builder_set_member_name(b, "sidebar_dir");  json_builder_add_string_value(b, s->sidebar_dir ? s->sidebar_dir : "");
	json_builder_set_member_name(b, "sidebar_visible"); json_builder_add_boolean_value(b, s->sidebar_visible);

	json_builder_end_object(b);

	root = json_builder_get_root(b);
	gen = json_generator_new();
	json_generator_set_root(gen, root);
	json_generator_set_pretty(gen, TRUE);
	json_generator_set_indent(gen, 2);
	data = json_generator_to_data(gen, NULL);

	path = settings_path();
	if (!g_file_set_contents(path, data, -1, &err)) {
		if (err) { g_printerr("l3afpad: can't save settings: %s\n", err->message); g_error_free(err); }
	}
	g_free(path);

	g_free(data);
	g_object_unref(gen);
	json_node_free(root);
	g_object_unref(b);
}

void settings_clear(Settings *s)
{
	g_free(s->fontname);    s->fontname = NULL;
	g_free(s->theme);       s->theme = NULL;
	g_free(s->sidebar_dir); s->sidebar_dir = NULL;
}
