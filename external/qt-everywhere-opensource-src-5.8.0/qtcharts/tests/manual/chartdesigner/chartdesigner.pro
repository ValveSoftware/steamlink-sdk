!include( ../../tests.pri ) {
    error( "Couldn't find the test.pri file!" )
}

TEMPLATE = app
QT += core gui

SOURCES += \
    brushwidget.cpp \ 
    main.cpp \
    mainwindow.cpp \
    objectinspectorwidget.cpp \
    penwidget.cpp \
    engine.cpp


HEADERS += \
    brushwidget.h \
    mainwindow.h \
    objectinspectorwidget.h \
    penwidget.h \
    engine.h

!system_build:mac: QMAKE_POST_LINK += "$$MAC_POST_LINK_PREFIX $$MAC_TESTS_BIN_DIR"
