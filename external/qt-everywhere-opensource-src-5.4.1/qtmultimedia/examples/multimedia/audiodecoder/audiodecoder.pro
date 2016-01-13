TEMPLATE = app
TARGET = audiodecoder

HEADERS = \
    audiodecoder.h \
    wavefilewriter.h
SOURCES = main.cpp \
    audiodecoder.cpp \
    wavefilewriter.cpp

QT += multimedia
CONFIG += console

target.path = $$[QT_INSTALL_EXAMPLES]/multimedia/audiodecoder
INSTALLS += target
