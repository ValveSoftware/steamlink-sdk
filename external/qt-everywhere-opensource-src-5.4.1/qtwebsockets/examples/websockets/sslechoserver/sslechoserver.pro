QT       += core websockets
QT       -= gui

TARGET = sslechoserver
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    main.cpp \
    sslechoserver.cpp

HEADERS += \
    sslechoserver.h

OTHER_FILES += sslechoclient.html
