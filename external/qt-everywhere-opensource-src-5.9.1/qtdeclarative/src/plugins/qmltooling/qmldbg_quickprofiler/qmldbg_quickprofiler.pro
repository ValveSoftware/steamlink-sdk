TARGET = qmldbg_quickprofiler
QT    += qml-private quick-private core-private packetprotocol-private

PLUGIN_TYPE = qmltooling
PLUGIN_CLASS_NAME = QQuickProfilerAdapterFactory
load(qt_plugin)

INCLUDEPATH += $$PWD/../shared

SOURCES += \
    $$PWD/qquickprofileradapter.cpp \
    $$PWD/qquickprofileradapterfactory.cpp

HEADERS += \
    $$PWD/qquickprofileradapter.h \
    $$PWD/qquickprofileradapterfactory.h \
    $$PWD/../shared/qqmldebugpacket.h

OTHER_FILES += \
    qquickprofileradapter.json
