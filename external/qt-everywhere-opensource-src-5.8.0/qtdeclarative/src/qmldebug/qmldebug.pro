TARGET    =  QtQmlDebug
QT        =  core-private network packetprotocol-private qml-private
CONFIG    += static internal_module

load(qt_module)

SOURCES += \
    qqmldebugclient.cpp \
    qqmldebugconnection.cpp \
    qqmlenginecontrolclient.cpp \
    qqmlprofilerclient.cpp

HEADERS += \
    qqmldebugclient_p.h \
    qqmldebugclient_p_p.h \
    qqmldebugconnection_p.h \
    qqmlenginecontrolclient_p.h \
    qqmlenginecontrolclient_p_p.h \
    qqmleventlocation_p.h \
    qqmlprofilerclient_p.h \
    qqmlprofilerclient_p_p.h
