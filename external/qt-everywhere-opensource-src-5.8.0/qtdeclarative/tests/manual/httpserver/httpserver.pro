QT       = core network

TARGET = httpserver
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SERVER = $$PWD/../../auto/shared
INCLUDEPATH += $$SERVER

SOURCES += main.cpp $$SERVER/testhttpserver.cpp
HEADERS += $$SERVER/testhttpserver.h
