TARGETPATH = QtBluetooth

QT = core quick bluetooth

HEADERS += \
    qdeclarativebluetoothservice_p.h \
    qdeclarativebluetoothsocket_p.h \
    qdeclarativebluetoothdiscoverymodel_p.h

SOURCES += plugin.cpp \
    qdeclarativebluetoothservice.cpp \
    qdeclarativebluetoothsocket.cpp \
    qdeclarativebluetoothdiscoverymodel.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

load(qml_plugin)
