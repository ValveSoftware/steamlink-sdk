QT += core qml quick websockets

TARGET = qmlwebsocketclient

TEMPLATE = app

CONFIG   -= app_bundle

SOURCES += main.cpp

RESOURCES += data.qrc

OTHER_FILES += qml/qmlwebsocketclient/main.qml

target.path = $$[QT_INSTALL_EXAMPLES]/websockets/qmlwebsocketclient
INSTALLS += target
