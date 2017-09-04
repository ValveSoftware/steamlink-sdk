TARGET = qtquickcontrols2plugin
TARGETPATH = QtQuick/Controls.2
IMPORT_VERSION = 2.2

QT += qml quick
QT_PRIVATE += core-private gui-private qml-private quick-private quicktemplates2-private quickcontrols2-private

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

include(controls.pri)

OTHER_FILES += \
    qmldir \
    $$QML_CONTROLS

SOURCES += \
    $$PWD/qtquickcontrols2plugin.cpp

RESOURCES += \
    $$PWD/qtquickcontrols2plugin.qrc

!static: qtConfig(quick-designer): include(designer/designer.pri)
include(doc/doc.pri)

qtquickcompiler {
    qmlfiles.prefix = /qt-project.org/imports/QtQuick/Controls.2
    qmlfiles.files += $$QML_CONTROLS
    RESOURCES += qmlfiles
} else:!static {
    CONFIG += qmlcache
}

CONFIG += no_cxx_module
load(qml_plugin)

requires(qtConfig(quickcontrols2-default))
