TEMPLATE = app

QT += qml scxml
CONFIG += c++11

SOURCES += mediaplayer-qml-static.cpp

RESOURCES += mediaplayer-qml-static.qrc

STATECHARTS = ../mediaplayer-common/mediaplayer.scxml

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/mediaplayer-qml-static
INSTALLS += target

