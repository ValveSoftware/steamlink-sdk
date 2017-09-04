TARGET = QtUiTools
CONFIG += static

include(../lib/uilib/uilib.pri)

QMAKE_DOCS = $$PWD/doc/qtuitools.qdocconf

HEADERS += quiloader.h
SOURCES += quiloader.cpp

DEFINES += \
    QFORMINTERNAL_NAMESPACE \
    QT_DESIGNER_STATIC

# QtUiPlugins end up in designer for historical reasons. However, if
# designer isn't actually built, we need to claim the plugin type here.
!qtBuildPart(tools): \
    MODULE_PLUGIN_TYPES = designer

load(qt_module)
