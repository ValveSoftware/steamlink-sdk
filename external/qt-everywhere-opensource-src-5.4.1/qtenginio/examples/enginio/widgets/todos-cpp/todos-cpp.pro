QT += network gui widgets enginio

TARGET = todos
TEMPLATE = app

include(../../common/backendhelper/backendhelper.pri)

SOURCES += \
    main.cpp\
    mainwindow.cpp \
    todosmodel.cpp \

HEADERS += \
    mainwindow.h \
    todosmodel.h \
