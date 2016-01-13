include($$PWD/macextras-lib.pri)

load(qt_build_config)
QT_PRIVATE += gui-private core-private
TARGET = QtMacExtras
load(qt_module)
QMAKE_DOCS = $$PWD/doc/qtmacextras.qdocconf
