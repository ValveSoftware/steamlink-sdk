
AUTOMAKE_OPTIONS = 1.11

DOC_MODULE = libkmod

DOC_MODULE_VERSION = 3

DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.xml

DOC_SOURCE_DIR = $(top_srcdir)/libkmod

SCAN_OPTIONS = --ignore-decorators="__must_check|KMOD_EXPORT"

MKDB_OPTIONS = --xml-mode --output-format=xml --name-space kmod --tmpl-dir=.

MKTMPL_OPTIONS = --output-dir=.

MKHTML_OPTIONS = --path=$(abs_srcdir)/doc --path=$(abs_builddir)/doc

HFILE_GLOB = $(top_srcdir)/libkmod/libkmod.h
CFILE_GLOB = $(top_srcdir)/libkmod/libkmod.c $(top_srcdir)/libkmod/libkmod-module.c $(top_srcdir)/libkmod/libkmod-list.c

IGNORE_HFILES = macro.h \
		libkmod-internal.h \
		libkmod-util.h \
		libkmod-array.h \
		libkmod-hash.h \
		libkmod-index.h

content_files = version.xml

if ENABLE_GTK_DOC
include $(top_srcdir)/libkmod/docs/gtk-doc.make
else
EXTRA_DIST = libkmod-docs.xml libkmod-sections.txt
endif
