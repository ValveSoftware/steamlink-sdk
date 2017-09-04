SOURCES += main.cpp
win32-msvc201* {
    LIBS += runtimeobject.lib
    CONFIG += console
}
