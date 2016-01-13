TEMPLATE = app

DEFINES += ENGINIO_SAMPLE_NAME=\\\"image-gallery\\\"

include(../../common/backendhelper/qmlbackendhelper.pri)

QT += quick qml enginio
SOURCES += ../main.cpp

mac: CONFIG -= app_bundle

OTHER_FILES += *.qml

RESOURCES += \
    gallery.qrc
