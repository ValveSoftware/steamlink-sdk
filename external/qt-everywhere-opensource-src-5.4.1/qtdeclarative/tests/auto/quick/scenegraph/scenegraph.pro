CONFIG += testcase
TARGET = tst_scenegraph
SOURCES += tst_scenegraph.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private  testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

OTHER_FILES += \
    data/render_OutOfFloatRange.qml \
    data/simple.qml \
    data/render_ImageFiltering.qml
