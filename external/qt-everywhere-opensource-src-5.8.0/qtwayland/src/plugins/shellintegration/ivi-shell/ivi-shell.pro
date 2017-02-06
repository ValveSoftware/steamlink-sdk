PLUGIN_TYPE = wayland-shell-integration
load(qt_plugin)

QT += gui-private waylandclient-private
CONFIG += wayland-scanner

QMAKE_USE += wayland-client

qtConfig(xkbcommon-evdev): \
    QMAKE_USE += xkbcommon_evdev

WAYLANDCLIENTSOURCES += \
    ../../../3rdparty/protocol/ivi-application.xml \
    ../../../3rdparty/protocol/ivi-controller.xml

HEADERS += \
    qwaylandivishellintegration.h \
    qwaylandivisurface_p.h

SOURCES += \
    main.cpp \
    qwaylandivishellintegration.cpp \
    qwaylandivisurface.cpp

OTHER_FILES += \
    ivi-shell.json
