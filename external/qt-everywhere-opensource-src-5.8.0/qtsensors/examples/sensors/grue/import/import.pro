TEMPLATE = lib
CONFIG += plugin

TARGET = $$qtLibraryTarget(declarative_grue)
DESTDIR = ../Grue

QT = core gui qml sensors

INCLUDEPATH += $$PWD/../lib
LIBS += -L$$OUT_PWD/.. -lgruesensor

# Shared gruesensor library will be installed in parent directory.
# Define rpath so that this plugin will know where to look for it.
unix:!mac: QMAKE_LFLAGS += -Wl,-rpath,\\\$\$ORIGIN/..

SOURCES = main.cpp

DESTPATH=$$[QT_INSTALL_EXAMPLES]/sensors/grue/Grue

target.path=$$DESTPATH
INSTALLS += target

CONFIG += install_ok  # Do not cargo-cult this!

qmldir.files=$$PWD/qmldir
qmldir.path=$$DESTPATH
INSTALLS += qmldir

OTHER_FILES += \
    import.json qmldir

# Copy the qmldir file to the same folder as the plugin binary
cpqmldir.files = $$PWD/qmldir
cpqmldir.path = $$DESTDIR
COPIES += cpqmldir
