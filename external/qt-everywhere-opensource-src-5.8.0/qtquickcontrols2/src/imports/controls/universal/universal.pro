TARGET = qtquickcontrols2universalstyleplugin
TARGETPATH = QtQuick/Controls.2/Universal
IMPORT_VERSION = 2.1

QT += qml quick
QT_PRIVATE += core-private gui-private qml-private quick-private quicktemplates2-private quickcontrols2-private

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

OTHER_FILES += \
    qmldir

SOURCES += \
    $$PWD/qtquickcontrols2universalstyleplugin.cpp

RESOURCES += \
    $$PWD/qtquickcontrols2universalstyleplugin.qrc

include(universal.pri)

CONFIG += no_cxx_module
load(qml_plugin)
