TARGET   = Qt3DQuick
MODULE   = 3dquick

QT      += core-private gui-private qml qml-private quick quick-private 3dcore 3dcore-private

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

gcov {
    QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
    QMAKE_LFLAGS += -fprofile-arcs -ftest-coverage
}

HEADERS += \
    qt3dquick_global.h \
    qt3dquick_global_p.h \
    qt3dquickvaluetypes_p.h \
    qt3dquicknodefactory_p.h \
    qqmlaspectengine.h \
    qqmlaspectengine_p.h \
    qquaternionanimation_p.h

SOURCES += \
    qt3dquick_global.cpp \
    qt3dquickvaluetypes.cpp \
    qt3dquicknodefactory.cpp \
    qqmlaspectengine.cpp \
    qquaternionanimation.cpp

# otherwise mingw headers do not declare common functions like ::strcasecmp
win32-g++*:QMAKE_CXXFLAGS_CXX11 = -std=gnu++0x

include(./items/items.pri)

load(qt_module)
