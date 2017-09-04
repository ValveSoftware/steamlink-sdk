requires(contains(QT_CONFIG, accessibility))

TARGET = qtquickextrasplugin
TARGETPATH = QtQuick/Extras
IMPORT_VERSION = 1.4

QT += qml

QMAKE_DOCS = $$PWD/doc/qtquickextras.qdocconf

QML_FILES += \
    CircularGauge.qml \
    DelayButton.qml \
    Dial.qml \
    Gauge.qml \
    StatusIndicator.qml \
    PieMenu.qml \
    ToggleButton.qml \
    Tumbler.qml \
    TumblerColumn.qml

HEADERS += plugin.h \
    qquicktriggermode_p.h \
    qquickpicture_p.h
SOURCES += plugin.cpp \
    qquickpicture.cpp

include(Private/private.pri)
include(designer/designer.pri)

!static: CONFIG += qmlcache

OTHER_FILES += doc/src/*

TR_EXCLUDE += designer/*

CONFIG += no_cxx_module
load(qml_plugin)
