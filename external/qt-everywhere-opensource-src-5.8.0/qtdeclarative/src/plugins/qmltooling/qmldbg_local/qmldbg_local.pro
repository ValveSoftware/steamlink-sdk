TARGET = qmldbg_local
QT = qml-private

SOURCES += \
    $$PWD/qlocalclientconnection.cpp

HEADERS += \
    $$PWD/qlocalclientconnectionfactory.h \
    $$PWD/../shared/qqmldebugserver.h \
    $$PWD/../shared/qqmldebugserverconnection.h

INCLUDEPATH += $$PWD \
    $$PWD/../shared

OTHER_FILES += \
    $$PWD/qlocalclientconnection.json

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QLocalClientConnectionFactory
load(qt_plugin)
