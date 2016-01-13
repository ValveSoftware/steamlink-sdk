TARGET = brcm_egl
CONFIG -= qt

INCLUDEPATH += $$QMAKE_INCDIR_EGL

for(p, QMAKE_LIBDIR_EGL) {
    exists($$p):LIBS += -L$$p
}

LIBS += $$QMAKE_LIBS_EGL

# Input
SOURCES += main.cpp
