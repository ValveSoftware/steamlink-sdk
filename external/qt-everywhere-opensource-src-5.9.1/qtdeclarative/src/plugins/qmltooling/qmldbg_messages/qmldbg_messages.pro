TARGET = qmldbg_messages
QT = qml-private core packetprotocol-private

SOURCES += \
    $$PWD/qdebugmessageservice.cpp \
    $$PWD/qdebugmessageservicefactory.cpp

HEADERS += \
    $$PWD/../shared/qqmldebugpacket.h \
    $$PWD/qdebugmessageservice.h \
    $$PWD/qdebugmessageservicefactory.h

INCLUDEPATH += $$PWD \
    $$PWD/../shared

OTHER_FILES += \
    $$PWD/qdebugmessageservice.json

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QDebugMessageServiceFactory
load(qt_plugin)
