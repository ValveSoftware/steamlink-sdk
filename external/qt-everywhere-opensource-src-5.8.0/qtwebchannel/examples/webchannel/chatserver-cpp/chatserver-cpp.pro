TARGET = chatserver

TEMPLATE = app

QT       += core websockets webchannel
QT       -= gui

CONFIG   += console

SOURCES += main.cpp \
    chatserver.cpp \
    ../shared/websocketclientwrapper.cpp \
    ../shared/websockettransport.cpp

HEADERS += \
    chatserver.h \
    ../shared/websocketclientwrapper.h \
    ../shared/websockettransport.h

target.path = $$[QT_INSTALL_EXAMPLES]/webchannel/chatserver-cpp
INSTALLS += target
