QT += network help-private

QTPLUGIN.platforms = qminimal

SOURCES += ../shared/helpgenerator.cpp \
           main.cpp

HEADERS += ../shared/helpgenerator.h

load(qt_tool)
