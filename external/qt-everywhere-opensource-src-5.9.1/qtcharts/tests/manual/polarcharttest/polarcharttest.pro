!include( ../../tests.pri ) {
    error( "Couldn't find the test.pri file!" )
}

QT       += core gui widgets

TARGET = polarcharttest
TEMPLATE = app
INCLUDEPATH += .


SOURCES += main.cpp \
            mainwindow.cpp \
            chartview.cpp

HEADERS  += mainwindow.h \
            chartview.h

FORMS    += mainwindow.ui
