TEMPLATE = aux

DISTFILES += test.qdocconf overview.qdoc data/qt_attribution.json data/LICENSE

run_qtattributionsscanner.commands = $$[QT_HOST_BINS]/qtattributionsscanner \
    --filter QDocModule=somemodule -o generated.qdoc $$PWD

run_docs.commands = BUILDDIR=$$OUT_PWD $$[QT_HOST_BINS]/qdoc $$PWD/test.qdocconf

check.depends = run_qtattributionsscanner run_docs

QMAKE_EXTRA_TARGETS += run_qtattributionsscanner run_docs check

QMAKE_CLEAN += generated.qdoc
