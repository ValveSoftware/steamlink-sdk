TEMPLATE = subdirs

QMAKE_DOCS = $$PWD/doc/qtspeech.qdocconf
load(qt_docs)

SUBDIRS = tts plugins

plugins.depends = tts
