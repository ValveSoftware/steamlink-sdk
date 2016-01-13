TARGET = resourceqt

QT += multimedia-private
CONFIG += no_private_qt_headers_warning link_pkgconfig
PKGCONFIG += libresourceqt5

PLUGIN_TYPE = resourcepolicy
PLUGIN_CLASS_NAME = ResourceQtPolicyPlugin
load(qt_plugin)

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

