TEMPLATE = app

QT += qml scxml
CONFIG += c++11

SOURCES += trafficlight-qml-simple.cpp

RESOURCES += trafficlight-qml-simple.qrc

STATECHARTS = ../trafficlight-common/statemachine.scxml

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/trafficlight-qml-simple
INSTALLS += target

