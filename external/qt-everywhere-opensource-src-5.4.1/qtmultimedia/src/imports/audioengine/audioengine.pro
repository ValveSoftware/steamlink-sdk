CXX_MODULE = multimedia
TARGET  = declarative_audioengine
TARGETPATH = QtAudioEngine
IMPORT_VERSION = 1.0

QT += quick qml multimedia-private

win32: LIBS += -lOpenAL32
unix:!mac:!blackberry: LIBS += -lopenal
blackberry: LIBS += -lOpenAL
mac: LIBS += -framework OpenAL
mac: DEFINES += HEADER_OPENAL_PREFIX

INCLUDEPATH += ../../multimedia/audio

HEADERS += \
        qdeclarative_attenuationmodel_p.h \
        qdeclarative_audioengine_p.h \
        qdeclarative_soundinstance_p.h \
        qdeclarative_audiocategory_p.h \
        qdeclarative_audiolistener_p.h \
        qdeclarative_playvariation_p.h \
        qdeclarative_audiosample_p.h \
        qdeclarative_sound_p.h \
        qsoundinstance_p.h \
        qaudioengine_p.h \
        qsoundsource_p.h \
        qsoundbuffer_p.h \
        qaudioengine_openal_p.h

SOURCES += \
        audioengine.cpp \
        qdeclarative_attenuationmodel_p.cpp \
        qdeclarative_audioengine_p.cpp \
        qdeclarative_soundinstance_p.cpp \
        qdeclarative_audiocategory_p.cpp \
        qdeclarative_audiolistener_p.cpp \
        qdeclarative_playvariation_p.cpp \
        qdeclarative_audiosample_p.cpp \
        qdeclarative_sound_p.cpp \
        qsoundinstance_p.cpp \
        qaudioengine_p.cpp \
        qsoundsource_openal_p.cpp  \
        qaudioengine_openal_p.cpp

load(qml_plugin)
