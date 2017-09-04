option(host_build)

QT = core-private

include(qscxmlc.pri)

TARGET = qscxmlc
CONFIG += console c++11

SOURCES += \
    main.cpp

load(qt_tool)
load(resources)

RESOURCES += templates.qrc
