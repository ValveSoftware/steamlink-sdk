TARGET = qtexttospeech_speechios
PLUGIN_TYPE = texttospeech
PLUGIN_CLASS_NAME = QTextToSpeechPluginIos

load(qt_plugin)

QT += core texttospeech
LIBS += -framework AVFoundation

HEADERS += \
    qtexttospeech_ios_plugin.h \
    qtexttospeech_ios.h \

OBJECTIVE_HEADERS += \
    qtexttospeech_ios.h \

SOURCES += \
    qtexttospeech_ios_plugin.cpp \

OBJECTIVE_SOURCES += \
    qtexttospeech_ios.mm \

OTHER_FILES += \
    ios_plugin.json
