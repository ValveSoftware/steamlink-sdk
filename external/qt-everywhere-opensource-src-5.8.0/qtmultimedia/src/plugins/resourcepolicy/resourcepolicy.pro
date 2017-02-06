TARGET = resourceqt

QT += multimedia-private

QMAKE_USE += libresourceqt5

INCLUDEPATH += $$PWD \
    $${SOURCE_DIR}/src/multimedia

HEADERS += \
    $$PWD/resourcepolicyplugin.h \
    $$PWD/resourcepolicyimpl.h \
    $$PWD/resourcepolicyint.h

SOURCES += \
    $$PWD/resourcepolicyplugin.cpp \
    $$PWD/resourcepolicyimpl.cpp \
    $$PWD/resourcepolicyint.cpp

PLUGIN_TYPE = resourcepolicy
PLUGIN_CLASS_NAME = ResourceQtPolicyPlugin
load(qt_plugin)
