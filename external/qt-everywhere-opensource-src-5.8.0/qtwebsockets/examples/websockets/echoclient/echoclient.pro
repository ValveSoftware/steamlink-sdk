QT       += core websockets
QT       -= gui

TARGET = echoclient
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    main.cpp \
    echoclient.cpp

HEADERS += \
    echoclient.h

target.path = $$[QT_INSTALL_EXAMPLES]/websockets/echoclient
INSTALLS += target
