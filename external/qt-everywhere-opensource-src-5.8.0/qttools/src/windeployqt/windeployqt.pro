option(host_build)
QT = core-private
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

SOURCES += main.cpp utils.cpp qmlutils.cpp elfreader.cpp
HEADERS += utils.h qmlutils.h elfreader.h

CONFIG += force_bootstrap

win32: LIBS += -lshlwapi

load(qt_tool)
