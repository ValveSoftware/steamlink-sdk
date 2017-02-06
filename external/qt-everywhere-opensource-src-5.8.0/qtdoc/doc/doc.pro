TEMPLATE = aux

QMAKE_DOCS = $$PWD/config/qtdoc.qdocconf

# FIXME: Refactor into load(qt_docs) or something similar
# that can be used from all non-module projects that also
# provide modularized docs, for example qmake.
QTDIR = $$[QT_HOST_PREFIX]
exists($$QTDIR/.qmake.cache): \
    QMAKE_DOCS_OUTPUTDIR = $$QTDIR/doc/qtdoc
else: \
    QMAKE_DOCS_OUTPUTDIR = $$OUT_PWD/qtdoc
