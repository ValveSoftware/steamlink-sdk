TARGET = qtexttospeech_winrt
PLUGIN_TYPE = texttospeech
PLUGIN_CLASS_NAME = QTextToSpeechPluginWinRT

load(qt_plugin)

QT += core core-private texttospeech

HEADERS += \
    qtexttospeech_winrt.h \
    qtexttospeech_winrt_plugin.h \

SOURCES += \
    qtexttospeech_winrt.cpp \
    qtexttospeech_winrt_plugin.cpp \

OTHER_FILES += \
    winrt_plugin.json
