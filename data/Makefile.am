SUBDIRS = icons

desktopdir       = $(datadir)/applications
desktop_in_files = l3afpad.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

pixmapsdir       = $(datadir)/pixmaps
pixmaps_DATA     = l3afpad.png l3afpad.xpm

pkgdatadir       = $(datadir)/l3afpad
pkgdata_DATA     = themes.tsv

schemesdir       = $(datadir)/l3afpad/schemes
schemes_DATA     = $(wildcard schemes/*.xml)
