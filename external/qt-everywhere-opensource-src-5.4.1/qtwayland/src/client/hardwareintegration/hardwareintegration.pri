WAYLANDCLIENTSOURCES += \
            $$PWD/../../extensions/server-buffer-extension.xml \
            $$PWD/../../extensions/hardware-integration.xml

INCLUDEPATH += $$PWD

SOURCES += \
           $$PWD/qwaylandclientbufferintegration.cpp \
           $$PWD/qwaylandclientbufferintegrationplugin.cpp \
           $$PWD/qwaylandclientbufferintegrationfactory.cpp \
           $$PWD/qwaylandserverbufferintegration.cpp \
           $$PWD/qwaylandserverbufferintegrationplugin.cpp \
           $$PWD/qwaylandserverbufferintegrationfactory.cpp \
           $$PWD/qwaylandhardwareintegration.cpp

HEADERS += \
           $$PWD/qwaylandclientbufferintegration_p.h \
           $$PWD/qwaylandclientbufferintegrationplugin_p.h \
           $$PWD/qwaylandclientbufferintegrationfactory_p.h \
           $$PWD/qwaylandserverbufferintegration_p.h \
           $$PWD/qwaylandserverbufferintegrationplugin_p.h \
           $$PWD/qwaylandserverbufferintegrationfactory_p.h \
           $$PWD/qwaylandhardwareintegration_p.h

