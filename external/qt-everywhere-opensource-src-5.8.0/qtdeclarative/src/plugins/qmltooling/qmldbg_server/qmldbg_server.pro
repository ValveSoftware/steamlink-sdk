TARGET = qmldbg_server
QT = qml-private packetprotocol-private

SOURCES += \
    $$PWD/qqmldebugserver.cpp

HEADERS += \
    $$PWD/qqmldebugserverfactory.h \
    $$PWD/../shared/qqmldebugserver.h \
    $$PWD/../shared/qqmldebugserverconnection.h \
    $$PWD/../shared/qqmldebugpacket.h

INCLUDEPATH += $$PWD \
    $$PWD/../shared

OTHER_FILES += \
    qqmldebugserver.json

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QQmlDebugServerFactory
load(qt_plugin)
