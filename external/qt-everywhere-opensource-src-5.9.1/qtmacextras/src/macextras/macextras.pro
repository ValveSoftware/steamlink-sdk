TARGET = QtMacExtras

QT_PRIVATE += gui-private core-private

include($$PWD/macextras-lib.pri)

QMAKE_DOCS = $$PWD/doc/qtmacextras.qdocconf

load(qt_module)
