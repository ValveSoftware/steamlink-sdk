TARGET = qmldbg_tcp
QT = qml-private network

SOURCES += \
    $$PWD/qtcpserverconnection.cpp

HEADERS += \
    $$PWD/qtcpserverconnectionfactory.h \
    $$PWD/../shared/qqmldebugserver.h \
    $$PWD/../shared/qqmldebugserverconnection.h

INCLUDEPATH += $$PWD \
    $$PWD/../shared

OTHER_FILES += \
    $$PWD/qtcpserverconnection.json

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QTcpServerConnectionFactory
load(qt_plugin)
