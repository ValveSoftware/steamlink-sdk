TEMPLATE = app

QT += qml scxml
CONFIG += c++11

SOURCES += mediaplayer-qml-dynamic.cpp

RESOURCES += mediaplayer-qml-dynamic.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/mediaplayer-qml-dynamic
INSTALLS += target

