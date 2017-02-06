!include( ../../tests.pri ) {
    error( "Couldn't find the test.pri file!" )
}

TEMPLATE = app

QT += core gui widgets

SOURCES += main.cpp \
    mainwidget.cpp \
    dataseriedialog.cpp

HEADERS += \
    mainwidget.h \
    dataseriedialog.h
