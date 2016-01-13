TARGET = qmldbg_qtquick2
QT       += qml-private quick-private core-private gui-private

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QtQuick2Plugin
load(qt_plugin)

INCLUDEPATH *= $$PWD $$PWD/../shared

SOURCES += \
    qtquick2plugin.cpp \
    highlight.cpp \
    qquickviewinspector.cpp \
    ../shared/abstracttool.cpp \
    ../shared/abstractviewinspector.cpp \
    inspecttool.cpp

HEADERS += \
    qtquick2plugin.h \
    highlight.h \
    qquickviewinspector.h \
    ../shared/abstracttool.h \
    ../shared/abstractviewinspector.h \
    ../shared/qmlinspectorconstants.h \
    inspecttool.h

OTHER_FILES += qtquick2plugin.json
