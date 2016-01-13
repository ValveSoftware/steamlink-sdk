isEmpty(QT_WAYLAND_GL_CONFIG):QT_WAYLAND_GL_CONFIG = $$(QT_WAYLAND_GL_CONFIG)

!isEqual(QT_WAYLAND_GL_CONFIG,nogl) {
    CONFIG += wayland-scanner
    WAYLANDSERVERSOURCES += \
        ../extensions/server-buffer-extension.xml \
        ../extensions/hardware-integration.xml

    HEADERS += \
        hardware_integration/qwlclientbufferintegration_p.h \
        hardware_integration/qwlclientbufferintegrationfactory_p.h \
        hardware_integration/qwlclientbufferintegrationplugin_p.h \
        hardware_integration/qwlserverbufferintegration_p.h \
        hardware_integration/qwlserverbufferintegrationfactory_p.h \
        hardware_integration/qwlserverbufferintegrationplugin_p.h \
        hardware_integration/qwlhwintegration_p.h

    SOURCES += \
        hardware_integration/qwlclientbufferintegration.cpp \
        hardware_integration/qwlclientbufferintegrationfactory.cpp \
        hardware_integration/qwlclientbufferintegrationplugin.cpp \
        hardware_integration/qwlserverbufferintegration.cpp \
        hardware_integration/qwlserverbufferintegrationfactory.cpp \
        hardware_integration/qwlserverbufferintegrationplugin.cpp \
        hardware_integration/qwlhwintegration.cpp

    DEFINES += QT_COMPOSITOR_WAYLAND_GL
} else {
    system(echo "Qt-Compositor configured as raster only compositor")
}
