TARGET   = Qt3DQuickRender
MODULE   = 3dquickrender

QT      += core core-private qml qml-private 3dcore 3drender 3dquick 3dquick-private 3dcore-private 3drender-private
CONFIG -= precompile_header

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

gcov {
    QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
    QMAKE_LFLAGS += -fprofile-arcs -ftest-coverage
}

SOURCES += \
    qt3dquickrender_global.cpp \
    qt3dquickrendernodefactory.cpp

HEADERS += \
    qt3dquickrendernodefactory_p.h \
    qt3dquickrender_global_p.h \
    qt3dquickrender_global.h

# otherwise mingw headers do not declare common functions like ::strcasecmp
win32-g++*:QMAKE_CXXFLAGS_CXX11 = -std=gnu++0x

include(./items/items.pri)

load(qt_module)
