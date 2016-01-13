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

OTHER_FILES += \
    plugin.json qmldir

copyfile = $$PWD/qmldir
copydest = $$DESTDIR

# On Windows, use backslashes as directory separators
equals(QMAKE_HOST.os, Windows) {
    copyfile ~= s,/,\\,g
    copydest ~= s,/,\\,g
}

# Copy the qmldir file to the same folder as the plugin binary
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$copyfile) $$quote($$copydest) $$escape_expand(\\n\\t)
