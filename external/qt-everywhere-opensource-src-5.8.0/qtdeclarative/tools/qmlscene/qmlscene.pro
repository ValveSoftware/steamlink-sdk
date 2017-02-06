QT += qml quick quick-private gui-private core-private
qtHaveModule(widgets): QT += widgets
CONFIG += no_import_scan

SOURCES += main.cpp

DEFINES += QML_RUNTIME_TESTING
!contains(QT_CONFIG, no-qml-debug): DEFINES += QT_QML_DEBUG_NO_WARNING

load(qt_tool)
