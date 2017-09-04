TARGET = qtexttospeech_sapi
PLUGIN_TYPE = texttospeech
PLUGIN_CLASS_NAME = QTextToSpeechPluginSapi

# sapi.h contains some parts that fail with "strictStrings"
QMAKE_CFLAGS_RELEASE -= -Zc:strictStrings
QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO -= -Zc:strictStrings
QMAKE_CXXFLAGS_RELEASE -= -Zc:strictStrings
QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO -= -Zc:strictStrings
QMAKE_CFLAGS -= -Zc:strictStrings
QMAKE_CXXFLAGS -= -Zc:strictStrings

load(qt_plugin)

QT += core texttospeech

mingw: LIBS += -luuid -lsapi -lole32

HEADERS += \
    qtexttospeech_sapi.h \
    qtexttospeech_sapi_plugin.h \

SOURCES += \
    qtexttospeech_sapi.cpp \
    qtexttospeech_sapi_plugin.cpp \

OTHER_FILES += \
    sapi_plugin.json
