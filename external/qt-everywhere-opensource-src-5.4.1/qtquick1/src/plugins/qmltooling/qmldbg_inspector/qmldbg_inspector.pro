TARGET = qmldbg_inspector
QT       += declarative declarative-private gui widgets core-private

PLUGIN_TYPE = qml1tooling
PLUGIN_CLASS_NAME = QtQuick1Plugin
load(qt_plugin)

INCLUDEPATH *= $$PWD

SOURCES += \
    qtquick1plugin.cpp \
    abstractviewinspector.cpp \
    qdeclarativeviewinspector.cpp \
    editor/abstractliveedittool.cpp \
    editor/liveselectiontool.cpp \
    editor/livelayeritem.cpp \
    editor/livesingleselectionmanipulator.cpp \
    editor/liverubberbandselectionmanipulator.cpp \
    editor/liveselectionrectangle.cpp \
    editor/liveselectionindicator.cpp \
    editor/boundingrecthighlighter.cpp \
    editor/subcomponentmasklayeritem.cpp \
    editor/zoomtool.cpp \
    editor/colorpickertool.cpp \
    abstracttool.cpp

HEADERS += \
    qtquick1plugin.h \
    abstractviewinspector.h \
    qdeclarativeinspectorprotocol.h \
    qdeclarativeviewinspector.h \
    qdeclarativeviewinspector_p.h \
    qmlinspectorconstants.h \
    editor/abstractliveedittool.h \
    editor/liveselectiontool.h \
    editor/livelayeritem.h \
    editor/livesingleselectionmanipulator.h \
    editor/liverubberbandselectionmanipulator.h \
    editor/liveselectionrectangle.h \
    editor/liveselectionindicator.h \
    editor/boundingrecthighlighter.h \
    editor/subcomponentmasklayeritem.h \
    editor/zoomtool.h \
    editor/colorpickertool.h \
    abstracttool.h

OTHER_FILES += qtquick1plugin.json
