TEMPLATE = lib
CONFIG += plugin
QT += qml

DESTDIR +=  ../imports/FileDialog
OBJECTS_DIR = tmp
MOC_DIR = tmp

TARGET = filedialogplugin

HEADERS += \
        directory.h \
        file.h \
        dialogPlugin.h

SOURCES += \
        directory.cpp \
        file.cpp \
        dialogPlugin.cpp

EXAMPLE_FILES += qmldir

# Copy the qmldir file to the same folder as the plugin binary
cpqmldir.files = $$PWD/qmldir
cpqmldir.path = $$DESTDIR
COPIES += cpqmldir

CONFIG += install_ok  # Do not cargo-cult this!
