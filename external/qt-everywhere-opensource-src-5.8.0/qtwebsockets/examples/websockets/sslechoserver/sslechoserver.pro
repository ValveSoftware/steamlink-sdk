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

EXAMPLE_FILES += sslechoclient.html

RESOURCES += securesocketclient.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/websockets/sslechoserver
INSTALLS += target
