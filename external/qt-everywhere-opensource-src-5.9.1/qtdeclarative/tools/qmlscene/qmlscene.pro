QT += qml quick quick-private gui-private core-private
qtHaveModule(widgets): QT += widgets
CONFIG += no_import_scan

SOURCES += main.cpp

DEFINES += QML_RUNTIME_TESTING
!contains(QT_CONFIG, no-qml-debug): DEFINES += QT_QML_DEBUG_NO_WARNING

QMAKE_TARGET_PRODUCT = qmlscene
QMAKE_TARGET_DESCRIPTION = Utility that loads and displays QML documents

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

load(qt_tool)
