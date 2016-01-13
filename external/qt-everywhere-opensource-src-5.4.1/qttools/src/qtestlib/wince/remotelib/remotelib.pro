TEMPLATE = lib
CONFIG += dll
CONFIG -= staticlib
TARGET = QtRemote
DESTDIR = ../../../../lib
QT =

HEADERS += commands.h
SOURCES += commands.cpp

INCLUDEPATH += $$QT_CE_RAPI_INC
LIBS += -L$$QT_CE_RAPI_LIB
