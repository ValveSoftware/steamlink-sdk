QT       += remoteobjects core

QT       -= gui

TARGET = directconnectserver
CONFIG   += console
CONFIG   -= app_bundle

REPC_SOURCE = simpleswitch.rep

TEMPLATE = app

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/simpleswitch/directconnectserver
INSTALLS += target

SOURCES += main.cpp \
    simpleswitch.cpp


HEADERS += \
    simpleswitch.h
