CXX_MODULE = declarative
TARGET = qmlshadersplugin
TARGETPATH = Qt/labs/shaders

QT += declarative widgets opengl

SOURCES += \
    qmlshadersplugin_plugin.cpp \
    shadereffect.cpp \
    shadereffectitem.cpp \
    shadereffectsource.cpp \
    scenegraph/qsggeometry.cpp \
    shadereffectbuffer.cpp

HEADERS += \
    qmlshadersplugin_plugin.h \
    glfunctions.h \
    shadereffect.h \
    shadereffectitem.h \
    shadereffectsource.h \
    scenegraph/qsggeometry.h \
    shadereffectbuffer.h

OTHER_FILES += shaders.json

load(qml1_plugin)
