#-------------------------------------------------
#
# Project created by QtCreator 2013-06-11T13:13:18
#
#-------------------------------------------------

QT       += core gui quick

TARGET = graph

TEMPLATE = app

SOURCES += main.cpp \
    graph.cpp \
    noisynode.cpp \
    gridnode.cpp \
    linenode.cpp

HEADERS += \
    graph.h \
    noisynode.h \
    gridnode.h \
    linenode.h

RESOURCES += \
    graph.qrc

OTHER_FILES += \
    main.qml \
    shaders/noisy.vsh \
    shaders/noisy.fsh \
    shaders/line.fsh \
    shaders/line.vsh

