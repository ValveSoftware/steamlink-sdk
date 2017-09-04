TEMPLATE = app

QT += qml scxml
CONFIG += c++11

SOURCES += trafficlight-qml-static.cpp

RESOURCES += trafficlight-qml-static.qrc

STATECHARTS = ../trafficlight-common/statemachine.scxml

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/trafficlight-qml-static
INSTALLS += target

