TARGET   = Qt3DQuickExtras
MODULE   = 3dquickextras

QT      += core core-private qml qml-private 3dcore 3dinput 3dquick 3drender 3drender-private 3dlogic
CONFIG -= precompile_header

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

gcov {
    QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
    QMAKE_LFLAGS += -fprofile-arcs -ftest-coverage
}

SOURCES += \
    qt3dquickwindow.cpp


HEADERS += \
    qt3dquickextras_global.h \
    qt3dquickwindow.h

# otherwise mingw headers do not declare common functions like ::strcasecmp
win32-g++*:QMAKE_CXXFLAGS_CXX11 = -std=gnu++0x

load(qt_module)
