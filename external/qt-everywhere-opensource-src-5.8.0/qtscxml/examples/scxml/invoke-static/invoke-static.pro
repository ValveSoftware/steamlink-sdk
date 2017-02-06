TEMPLATE = app

QT += qml scxml
CONFIG += c++11

SOURCES += invoke-static.cpp

RESOURCES += invoke-static.qrc

STATECHARTS = ../invoke-common/statemachine.scxml

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/invoke-static
INSTALLS += target

