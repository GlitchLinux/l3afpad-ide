#!/usr/bin/env python3
"""Convert rainglow .tmTheme (plist) color themes to GtkSourceView 4
style-scheme XML files.

Usage: tmtheme2gsv.py <indir-with-xml-tmthemes> <outdir>
"""
import plistlib, glob, os, sys, html

# GtkSourceView style name -> tmTheme scopes (priority order, prefix match)
SCOPE_MAP = {
    "def:comment":          ["comment"],
    "def:string":           ["string"],
    "def:keyword":          ["keyword"],
    "def:statement":        ["keyword.control", "storage"],
    "def:type":             ["storage.type", "entity.name.class",
                             "support.type", "support.class"],
    "def:function":         ["entity.name.function", "support.function"],
    "def:identifier":       ["variable", "variable.parameter"],
    "def:decimal":          ["constant.numeric"],
    "def:floating-point":   ["constant.numeric"],
    "def:base-n-integer":   ["constant.numeric"],
    "def:special-constant": ["constant.language", "constant.other"],
    "def:constant":         ["constant"],
    "def:preprocessor":     ["meta.preprocessor", "keyword.other"],
    "def:error":            ["invalid"],
    "def:operator":         ["keyword.operator"],
}

def norm(c):
    if not c:
        return None
    c = c.strip()
    if not c.startswith("#"):
        return None
    c = c[:7]
    return c.lower() if len(c) == 7 else None

def load_theme(path):
    with open(path, "rb") as fh:
        p = plistlib.load(fh)
    name = p.get("name", os.path.basename(path)).replace(" (rainglow)", "")
    base, scopes = {}, {}
    for entry in p.get("settings", []):
        st = entry.get("settings", {})
        if "scope" not in entry and ("background" in st or "foreground" in st):
            base = st
            continue
        scope = entry.get("scope", "")
        if not scope:
            continue
        for sc in [s.strip() for s in scope.split(",")]:
            scopes[sc] = st
    return name, base, scopes

def find_scope(scopes, wanted):
    for w in wanted:
        if w in scopes:
            st = scopes[w]
            return norm(st.get("foreground")), st.get("fontStyle", "")
    for w in wanted:
        for sc, st in scopes.items():
            if sc.startswith(w):
                return norm(st.get("foreground")), st.get("fontStyle", "")
    return None, None

def style(name, fg=None, bg=None, bold=False, italic=False, underline=False):
    a = ['name="%s"' % name]
    if fg: a.append('foreground="%s"' % fg)
    if bg: a.append('background="%s"' % bg)
    if bold: a.append('bold="true"')
    if italic: a.append('italic="true"')
    if underline: a.append('underline="single"')
    return "    <style " + " ".join(a) + "/>"

def convert(path, outdir):
    name, base, scopes = load_theme(path)
    slug = os.path.basename(path)[:-4]
    sid = "rainglow-" + slug
    bg     = norm(base.get("background")) or "#000000"
    fg     = norm(base.get("foreground")) or "#ffffff"
    caret  = norm(base.get("caret")) or fg
    sel    = norm(base.get("selection")) or "#444444"
    linehl = norm(base.get("lineHighlight")) or bg

    L = []
    L.append('<?xml version="1.0" encoding="UTF-8"?>')
    L.append('<style-scheme id="%s" name="%s" version="1.0">' % (sid, html.escape(name)))
    L.append('  <author>rainglow (Dayle Rees), converted for l3afpad</author>')
    L.append('  <description>%s (rainglow)</description>' % html.escape(name))
    L.append('')
    L.append(style("text", fg=fg, bg=bg))
    L.append(style("selection", bg=sel))
    L.append(style("cursor", fg=caret))
    L.append(style("current-line", bg=linehl))
    L.append(style("line-numbers", fg=fg, bg=bg))
    L.append(style("bracket-match", bold=True))
    L.append('')
    for gsv, wanted in SCOPE_MAP.items():
        col, fstyle = find_scope(scopes, wanted)
        if not col:
            continue
        fstyle = (fstyle or "").lower()
        L.append(style(gsv, fg=col,
            bold=("bold" in fstyle),
            italic=("italic" in fstyle),
            underline=("underline" in fstyle)))
    L.append('</style-scheme>')
    with open(os.path.join(outdir, slug + ".xml"), "w") as fh:
        fh.write("\n".join(L) + "\n")

def main():
    indir, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    n = 0
    for f in sorted(glob.glob(os.path.join(indir, "*.xml"))):
        try:
            convert(f, outdir); n += 1
        except Exception as e:
            print("SKIP", f, e, file=sys.stderr)
    print("converted %d themes to %s" % (n, outdir))

if __name__ == "__main__":
    main()
