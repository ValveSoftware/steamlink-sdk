QT += qml scxml

CONFIG += c++11

SOURCES += calculator-qml.cpp

RESOURCES += calculator-qml.qrc

STATECHARTS = ../calculator-common/statemachine.scxml

# install
target.path = $$[QT_INSTALL_EXAMPLES]/scxml/calculator-qml
INSTALLS += target
