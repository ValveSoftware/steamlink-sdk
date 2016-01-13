# common to plugin and built-in forms
INCLUDEPATH *= $$PWD
HEADERS += $$PWD/qjp2handler_p.h
SOURCES += $$PWD/qjp2handler.cpp
config_jasper {
    msvc: LIBS += libjasper.lib
    else: LIBS += -ljasper
} else {
    include($$PWD/../../../3rdparty/jasper.pri)
}
