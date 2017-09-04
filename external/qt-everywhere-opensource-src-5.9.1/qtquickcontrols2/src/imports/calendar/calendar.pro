TARGET = qtlabscalendarplugin
TARGETPATH = Qt/labs/calendar
IMPORT_VERSION = 1.0

QT += qml quick
QT_PRIVATE += core-private gui-private qml-private quick-private quicktemplates2-private

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

QMAKE_DOCS = $$PWD/doc/qtlabscalendar.qdocconf

OTHER_FILES += \
    qmldir

SOURCES += \
    $$PWD/qtlabscalendarplugin.cpp

include(calendar.pri)

!static: CONFIG += qmlcache

CONFIG += no_cxx_module
load(qml_plugin)
