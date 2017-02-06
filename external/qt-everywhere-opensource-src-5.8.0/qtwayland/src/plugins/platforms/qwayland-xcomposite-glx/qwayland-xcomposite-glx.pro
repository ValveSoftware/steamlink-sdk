QT += waylandclient-private

include(../../../hardwareintegration/client/xcomposite-glx/xcomposite-glx.pri)

OTHER_FILES += qwayland-xcomposite-glx.json

SOURCES += \
    main.cpp

HEADERS += \
    qwaylandxcompositeglxplatformintegration.h

PLUGIN_TYPE = platforms
load(qt_plugin)
