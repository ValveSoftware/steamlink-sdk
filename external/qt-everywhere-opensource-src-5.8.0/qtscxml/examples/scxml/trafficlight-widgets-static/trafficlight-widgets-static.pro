QT += widgets scxml
CONFIG += c++11

SOURCES = ../trafficlight-common/trafficlight.cpp
HEADERS = ../trafficlight-common/trafficlight.h
STATECHARTS = ../trafficlight-common/statemachine.scxml

SOURCES += trafficlight-widgets-static.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/scxml/trafficlight-widgets-static
INSTALLS += target

RESOURCES += \
    trafficlight-widgets-static.qrc
