TARGET   = Qt3DQuickScene2D
MODULE   = 3dquickscene2d

QT      += core core-private qml qml-private 3dcore 3drender 3dquick 3dquick-private 3dcore-private 3drender-private
CONFIG -= precompile_header

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

gcov {
    QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
    QMAKE_LFLAGS += -fprofile-arcs -ftest-coverage
}

SOURCES += \
    qt3dquickscene2d_global.cpp \
    qt3dquickscene2dnodefactory.cpp \
    qt3dquickscene2d_logging.cpp

HEADERS += \
    qt3dquickscene2dnodefactory_p.h \
    qt3dquickscene2d_global_p.h \
    qt3dquickscene2d_global.h \
    qt3dquickscene2d_logging_p.h

# otherwise mingw headers do not declare common functions like ::strcasecmp
win32-g++*:QMAKE_CXXFLAGS_CXX11 = -std=gnu++0x

include(./items/items.pri)

load(qt_module)
