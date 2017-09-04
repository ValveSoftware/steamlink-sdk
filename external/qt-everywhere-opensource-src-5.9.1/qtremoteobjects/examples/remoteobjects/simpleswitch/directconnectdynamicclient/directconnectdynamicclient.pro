QT       += remoteobjects core

QT       -= gui

TARGET = directconnectdynamicclient
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/simpleswitch/directconnectdynamicclient
INSTALLS += target

SOURCES += main.cpp \
    dynamicclient.cpp

HEADERS += \
    dynamicclient.h
