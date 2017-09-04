TARGET = qmldbg_inspector
QT    += qml-private quick-private core-private gui-private packetprotocol-private

INCLUDEPATH *= $$PWD $$PWD/../shared

SOURCES += \
    $$PWD/globalinspector.cpp \
    $$PWD/highlight.cpp \
    $$PWD/inspecttool.cpp \
    $$PWD/qqmlinspectorservice.cpp \
    $$PWD/qquickwindowinspector.cpp

HEADERS += \
    $$PWD/../shared/qqmldebugpacket.h \
    $$PWD/globalinspector.h \
    $$PWD/highlight.h \
    $$PWD/inspecttool.h\
    $$PWD/qqmlinspectorservicefactory.h \
    $$PWD/qquickwindowinspector.h

OTHER_FILES += \
    qqmlinspectorservice.json

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QQmlInspectorServiceFactory
load(qt_plugin)
