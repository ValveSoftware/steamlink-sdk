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

QMAKE_TARGET_PRODUCT = qmlprofiler
QMAKE_TARGET_DESCRIPTION = QML profiler

win32 {
   VERSION = $${QT_VERSION}.0
} else {
   VERSION = $${QT_VERSION}
}

load(qt_tool)
