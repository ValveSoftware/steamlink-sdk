# Doc snippets - compiled for truthiness

TEMPLATE = lib
TARGET = qtmmksnippets

INCLUDEPATH += ../../../../src/global \
               ../../../../src/multimedia \
               ../../../../src/multimedia/audio \
               ../../../../src/multimedia/video \
               ../../../../src/multimedia/camera

CONFIG += console

QT += multimedia multimediawidgets widgets multimedia-private

SOURCES += \
    audio.cpp \
    video.cpp \
    camera.cpp \
    media.cpp \
    qsound.cpp

OTHER_FILES += \
    soundeffect.qml
