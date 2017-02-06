TARGET = Qt3DInput

DEFINES += QT3DINPUT_LIBRARY

MODULE = 3dinput
MODULE_PLUGIN_TYPES = 3dinputdevices

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

QT += core-private 3dcore-private

include(frontend/frontend.pri)
include(backend/backend.pri)

HEADERS += \
    qt3dinput_global.h \
    qt3dinput_global_p.h

load(qt_module)
