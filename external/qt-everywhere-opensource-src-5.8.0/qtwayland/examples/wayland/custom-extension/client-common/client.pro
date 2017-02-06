CONFIG += wayland-scanner
TARGET = custom-wayland

QT += waylandclient-private

WAYLANDCLIENTSOURCES += ../protocol/custom.xml

OTHER_FILES += client.json

SOURCES += main.cpp \
           customextension.cpp

HEADERS += customextension.h

PLUGIN_TYPE = platforms
load(qt_plugin)

# Installation into a "proper" Qt location is most unexpected for from an example.
CONFIG += install_ok
