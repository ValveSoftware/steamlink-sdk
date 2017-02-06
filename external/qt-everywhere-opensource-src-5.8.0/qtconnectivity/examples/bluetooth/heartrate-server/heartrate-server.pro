TEMPLATE = app
TARGET = heartrate-server

QT += bluetooth
CONFIG += c++11

SOURCES += main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/bluetooth/heartrate-server
INSTALLS += target
