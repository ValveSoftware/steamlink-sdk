TEMPLATE = app
TARGET = radio

QT += multimedia

HEADERS = \
    radio.h

SOURCES = \
    main.cpp \
    radio.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/multimedia/radio
INSTALLS += target

QT+=widgets
