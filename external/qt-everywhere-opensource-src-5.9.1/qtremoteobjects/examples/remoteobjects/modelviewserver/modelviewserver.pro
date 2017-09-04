QT += widgets remoteobjects

TEMPLATE = app

CONFIG += c++11

SOURCES += main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/modelviewserver
INSTALLS += target
