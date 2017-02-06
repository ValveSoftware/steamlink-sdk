TARGET = QtQuick

QT = core-private gui-private qml-private
qtConfig(qml-network): \
    QT_PRIVATE += network

DEFINES   += QT_NO_URL_CAST_FROM_STRING QT_NO_INTEGER_EVENT_COORDINATES
win32-msvc*:DEFINES *= _CRT_SECURE_NO_WARNINGS
solaris-cc*:QMAKE_CXXFLAGS_RELEASE -= -O2
win32:!winrt: LIBS += -luser32

exists("qqml_enable_gcov") {
    QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage -fno-elide-constructors
    LIBS_PRIVATE += -lgcov
}

QMAKE_DOCS = $$PWD/doc/qtquick.qdocconf

ANDROID_LIB_DEPENDENCIES = \
    lib/libQt5QuickParticles.so
MODULE_PLUGIN_TYPES += \
    accessible/libqtaccessiblequick.so \
    scenegraph
ANDROID_BUNDLED_FILES += \
    qml \
    lib/libQt5QuickParticles.so

include(util/util.pri)
include(scenegraph/scenegraph.pri)
include(items/items.pri)
qtConfig(quick-designer): \
    include(designer/designer.pri)
qtConfig(accessibility) {
    include(accessible/accessible.pri)
}

HEADERS += \
    qtquickglobal.h \
    qtquickglobal_p.h \
    qtquick2_p.h

SOURCES += qtquick2.cpp

# To make #include "qquickcontext2d_jsclass.cpp" work
INCLUDEPATH += $$PWD

load(qt_module)
