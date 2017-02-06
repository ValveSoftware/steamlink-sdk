TARGET = qtexttospeech_speechosx
PLUGIN_TYPE = texttospeech
PLUGIN_CLASS_NAME = QTextToSpeechPluginOsx

load(qt_plugin)

QT += core texttospeech

LIBS += -framework Cocoa

HEADERS += \
    qtexttospeech_osx_plugin.h \
    qtexttospeech_osx.h \

OBJECTIVE_HEADERS += \
    qtexttospeech_osx.h \

SOURCES += \
    qtexttospeech_osx_plugin.cpp \

OBJECTIVE_SOURCES += \
    qtexttospeech_osx.mm \

OTHER_FILES += \
    osx_plugin.json
