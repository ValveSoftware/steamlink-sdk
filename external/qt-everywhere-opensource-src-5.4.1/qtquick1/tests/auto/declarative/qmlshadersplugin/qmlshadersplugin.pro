CONFIG += testcase
TARGET = tst_qmlshadersplugin

QT += testlib opengl declarative gui widgets
SOURCES += tst_qmlshadersplugin.cpp

SOURCES += \
    ../../../../src/imports/shaders/shadereffectitem.cpp \
    ../../../../src/imports/shaders/shadereffectsource.cpp \
    ../../../../src/imports/shaders/shadereffect.cpp \
    ../../../../src/imports/shaders/shadereffectbuffer.cpp \
    ../../../../src/imports/shaders/scenegraph/qsggeometry.cpp

HEADERS += \
    ../../../../src/imports/shaders/shadereffectitem.h \
    ../../../../src/imports/shaders/shadereffectsource.h \
    ../../../../src/imports/shaders/shadereffect.h \
    ../../../../src/imports/shaders/shadereffectbuffer.h \
    ../../../../src/imports/shaders/scenegraph/qsggeometry.h

include(../shared/qdeclarativedatatest.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
