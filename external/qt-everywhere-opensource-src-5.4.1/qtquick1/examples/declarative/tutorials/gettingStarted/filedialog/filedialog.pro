TEMPLATE = lib
CONFIG += plugin
QT += declarative

DESTDIR +=  ../plugins
OBJECTS_DIR = tmp
MOC_DIR = tmp

TARGET = FileDialog

HEADERS +=     directory.h \
        file.h \
        dialogPlugin.h

SOURCES +=    directory.cpp \
        file.cpp \
        dialogPlugin.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/gettingStarted/filedialog
qml.files = qmldir
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/tutorials/gettingStarted/filedialog
INSTALLS += target qml
