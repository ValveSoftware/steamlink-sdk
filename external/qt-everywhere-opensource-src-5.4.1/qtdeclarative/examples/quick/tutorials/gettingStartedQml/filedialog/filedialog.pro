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

OTHER_FILES += qmldir

# Copy the qmldir file to the same folder as the plugin binary
QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$shell_path($$PWD/qmldir)) \
                                $$shell_quote($$shell_path($$DESTDIR)) $$escape_expand(\\n\\t)
