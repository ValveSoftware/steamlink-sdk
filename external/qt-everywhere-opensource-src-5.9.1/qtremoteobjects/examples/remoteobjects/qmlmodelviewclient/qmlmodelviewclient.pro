TEMPLATE = app

QT += qml quick remoteobjects
CONFIG += c++11

SOURCES += main.cpp

RESOURCES += qml.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/qmlmodelviewclient
INSTALLS += target
