TARGET = qmldbg_nativedebugger
QT = qml-private core packetprotocol-private

SOURCES += \
    $$PWD/qqmlnativedebugservicefactory.cpp \
    $$PWD/qqmlnativedebugservice.cpp

HEADERS += \
    $$PWD/../shared/qqmldebugpacket.h \
    $$PWD/qqmlnativedebugservicefactory.h \
    $$PWD/qqmlnativedebugservice.h \

INCLUDEPATH += $$PWD \
    $$PWD/../shared

OTHER_FILES += \
    $$PWD/qqmlnativedebugservice.json

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QQmlNativeDebugServiceFactory
load(qt_plugin)
