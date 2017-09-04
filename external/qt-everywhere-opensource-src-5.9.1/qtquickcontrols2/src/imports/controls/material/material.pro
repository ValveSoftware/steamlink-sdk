TARGET = qtquickcontrols2materialstyleplugin
TARGETPATH = QtQuick/Controls.2/Material
IMPORT_VERSION = 2.2

QT += qml quick
QT_PRIVATE += core-private gui-private qml-private quick-private quicktemplates2-private quickcontrols2-private

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

include(material.pri)

OTHER_FILES += \
    qmldir \
    $$QML_FILES

SOURCES += \
    $$PWD/qtquickcontrols2materialstyleplugin.cpp

RESOURCES += \
    $$PWD/qtquickcontrols2materialstyleplugin.qrc

!static: CONFIG += qmlcache
CONFIG += no_cxx_module
load(qml_plugin)

requires(qtConfig(quickcontrols2-material))
