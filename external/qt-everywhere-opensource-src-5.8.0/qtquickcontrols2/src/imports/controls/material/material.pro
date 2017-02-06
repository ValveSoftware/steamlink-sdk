TARGET = qtquickcontrols2materialstyleplugin
TARGETPATH = QtQuick/Controls.2/Material
IMPORT_VERSION = 2.1

QT += qml quick
QT_PRIVATE += core-private gui-private qml-private quick-private quicktemplates2-private quickcontrols2-private

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

OTHER_FILES += \
    qmldir

SOURCES += \
    $$PWD/qtquickcontrols2materialstyleplugin.cpp

RESOURCES += \
    $$PWD/qtquickcontrols2materialstyleplugin.qrc

include(material.pri)

CONFIG += no_cxx_module
load(qml_plugin)
