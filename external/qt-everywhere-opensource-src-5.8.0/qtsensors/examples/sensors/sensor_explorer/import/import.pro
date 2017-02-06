TEMPLATE = lib
CONFIG += plugin

TARGET = $$qtLibraryTarget(declarative_explorer)
DESTDIR = ../Explorer

QT += qml sensors

SOURCES = \
    main.cpp \
    explorer.cpp \
    sensoritem.cpp \
    propertyinfo.cpp

HEADERS = \
    explorer.h \
    sensoritem.h \
    propertyinfo.h

DESTPATH=$$[QT_INSTALL_EXAMPLES]/sensors/sensor_explorer/Explorer

target.path=$$DESTPATH
qmldir.files=$$PWD/qmldir
qmldir.path=$$DESTPATH
INSTALLS += target qmldir

CONFIG += install_ok  # Do not cargo-cult this!

OTHER_FILES += \
    import.json qmldir

# Copy the qmldir file to the same folder as the plugin binary
cpqmldir.files = $$PWD/qmldir
cpqmldir.path = $$DESTDIR
COPIES += cpqmldir
