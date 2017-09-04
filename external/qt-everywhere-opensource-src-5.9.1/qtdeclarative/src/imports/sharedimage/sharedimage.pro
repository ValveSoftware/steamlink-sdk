CXX_MODULE = qml
TARGET = sharedimageplugin
TARGETPATH = Qt/labs/sharedimage
IMPORT_VERSION = 1.0

QT *= quick-private qml gui-private core-private

SOURCES += \
    plugin.cpp \
    sharedimageprovider.cpp \
    qsharedimageloader.cpp

HEADERS += \
    sharedimageprovider.h \
    qsharedimageloader_p.h

load(qml_plugin)
