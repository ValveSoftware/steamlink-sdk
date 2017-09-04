SOURCES += main.cpp

QT += qml qmltest
CONFIG += no_import_scan

QMAKE_TARGET_PRODUCT = qmltestrunner
QMAKE_TARGET_DESCRIPTION = QML test runner

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

load(qt_tool)
