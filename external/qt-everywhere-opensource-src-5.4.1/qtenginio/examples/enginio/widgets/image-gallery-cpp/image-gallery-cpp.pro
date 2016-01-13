QT += network gui widgets enginio

TARGET = image-gallery
TEMPLATE = app

include(../../common/backendhelper/backendhelper.pri)

SOURCES += \
    main.cpp\
    mainwindow.cpp \
    imageobject.cpp \
    imagemodel.cpp

HEADERS += \
    mainwindow.h \
    imageobject.h \
    imagemodel.h
