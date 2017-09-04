CONFIG   += console


REPC_SOURCE += ../timemodel.rep
QT = remoteobjects core

SOURCES += timemodel.cpp main.cpp
HEADERS += timemodel.h

contains(QT_CONFIG, c++11): CONFIG += c++11

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/server
INSTALLS += target
