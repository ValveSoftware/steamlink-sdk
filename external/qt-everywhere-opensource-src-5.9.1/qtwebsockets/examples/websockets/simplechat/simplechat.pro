QT       += core websockets
QT       -= gui

TARGET = chatserver
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    main.cpp \
    chatserver.cpp

HEADERS += \
    chatserver.h

EXAMPLE_FILES += chatclient.html

target.path = $$[QT_INSTALL_EXAMPLES]/websockets/simplechat
INSTALLS += target
