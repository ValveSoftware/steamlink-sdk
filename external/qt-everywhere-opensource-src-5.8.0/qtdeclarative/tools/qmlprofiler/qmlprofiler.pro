QT = qml-private network core qmldebug-private
CONFIG += no_import_scan

SOURCES += main.cpp \
    qmlprofilerapplication.cpp \
    commandlistener.cpp \
    qmlprofilerdata.cpp \
    qmlprofilerclient.cpp

HEADERS += \
    qmlprofilerapplication.h \
    commandlistener.h \
    constants.h \
    qmlprofilerdata.h \
    qmlprofilerclient.h

load(qt_tool)
