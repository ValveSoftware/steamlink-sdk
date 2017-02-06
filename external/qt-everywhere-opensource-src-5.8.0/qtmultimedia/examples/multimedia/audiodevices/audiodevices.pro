TEMPLATE = app
TARGET = audiodevices

QT += multimedia

HEADERS       = audiodevices.h

SOURCES       = audiodevices.cpp \
                main.cpp

FORMS        += audiodevicesbase.ui

target.path = $$[QT_INSTALL_EXAMPLES]/multimedia/audiodevices
INSTALLS += target

QT+=widgets
