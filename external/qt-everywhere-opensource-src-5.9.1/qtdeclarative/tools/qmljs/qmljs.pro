TEMPLATE = app
QT = qml-private core-private
CONFIG += no_import_scan
SOURCES = qmljs.cpp

include($$PWD/../../src/3rdparty/masm/masm-defs.pri)

QMAKE_TARGET_PRODUCT = qmljs
QMAKE_TARGET_DESCRIPTION = QML Javascript tool

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

load(qt_tool)
