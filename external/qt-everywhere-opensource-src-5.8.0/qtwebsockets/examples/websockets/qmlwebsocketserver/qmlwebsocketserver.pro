QT += core qml quick websockets

TARGET = qmlwebsocketserver

TEMPLATE = app

CONFIG   -= app_bundle

SOURCES += main.cpp

RESOURCES += data.qrc

OTHER_FILES += qml/qmlwebsocketserver/main.qml

target.path = $$[QT_INSTALL_EXAMPLES]/websockets/qmlwebsocketserver
INSTALLS += target
