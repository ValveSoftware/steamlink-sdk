!include( ../../tests.pri ) {
    error( "Couldn't find the test.pri file!" )
}

QT += widgets

SOURCES += main.cpp \
    mainwidget.cpp \
    customtablemodel.cpp

HEADERS += \
    mainwidget.h \
    customtablemodel.h
