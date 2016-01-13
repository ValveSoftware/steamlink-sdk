# common to plugin and built-in forms
INCLUDEPATH *= $$PWD
HEADERS += $$PWD/qmnghandler_p.h
SOURCES += $$PWD/qmnghandler.cpp
config_libmng {
    unix|mingw: LIBS += -lmng
    else:win32: LIBS += libmng.lib
} else {
    include($$PWD/../../../3rdparty/libmng.pri)
    *-g++*: QMAKE_CFLAGS_WARN_ON += -Wno-unused-parameter
}
