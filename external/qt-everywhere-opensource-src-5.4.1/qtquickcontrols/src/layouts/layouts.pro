CXX_MODULE = qml
TARGET  = qquicklayoutsplugin
TARGETPATH = QtQuick/Layouts
IMPORT_VERSION = 1.1

QT *= qml-private quick-private gui-private core-private

QMAKE_DOCS = $$PWD/doc/qtquicklayouts.qdocconf

SOURCES += plugin.cpp \
    qquicklayout.cpp \
    qquicklinearlayout.cpp \
    qquickgridlayoutengine.cpp \
    qquicklayoutstyleinfo.cpp

HEADERS += \
    qquicklayout_p.h \
    qquicklinearlayout_p.h \
    qquickgridlayoutengine_p.h \
    qquicklayoutstyleinfo_p.h

load(qml_plugin)
