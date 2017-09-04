#-------------------------------------------------
#
# Project created by QtCreator 2015-02-13T15:02:06
#
#-------------------------------------------------

QT       += remoteobjects core

QT       -= gui

TARGET = registryconnectedserver
CONFIG   += console
CONFIG   -= app_bundle

REPC_SOURCE = simpleswitch.rep


TEMPLATE = app

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/simpleswitch/registryconnectedserver
INSTALLS += target

SOURCES += main.cpp \
    simpleswitch.cpp

HEADERS += \
    simpleswitch.h

DISTFILES += \
    simpleswitch.rep
