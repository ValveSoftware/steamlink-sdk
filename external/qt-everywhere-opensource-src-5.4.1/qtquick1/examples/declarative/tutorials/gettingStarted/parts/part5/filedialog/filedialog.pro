TEMPLATE = lib
CONFIG += plugin
QT += declarative

DESTDIR +=  ../plugins
OBJECTS_DIR = tmp
MOC_DIR = tmp

TARGET = FileDialog

HEADERS +=      directory.h \
                file.h \
                dialogPlugin.h

SOURCES +=      directory.cpp \
                file.cpp \
                dialogPlugin.cpp
