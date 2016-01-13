CONFIG   += console
QT       += network
QT       -= gui

INCLUDEPATH += ../cetcpsyncserver

SOURCES += main.cpp \
           remoteconnection.cpp \
           qtcesterconnection.cpp

HEADERS += \
           remoteconnection.h \
           qtcesterconnection.h

load(qt_app)
