TARGET = qttexttospeech_android
QT = core core-private texttospeech

PLUGIN_TYPE = texttospeech
PLUGIN_CLASS_NAME = QTextToSpeechEngineAndroid
load(qt_plugin)

HEADERS += \
    qtexttospeech_android.h \
    qtexttospeech_android_plugin.h

SOURCES += \
    qtexttospeech_android.cpp \
    qtexttospeech_android_plugin.cpp

OTHER_FILES += \
    android_plugin.json
