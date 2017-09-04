TARGET = QtQuickTemplates2
MODULE = quicktemplates2
CONFIG += internal_module

QT += quick
QT_PRIVATE += core-private gui-private qml-private quick-private

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

HEADERS += \
    $$PWD/qtquicktemplates2global_p.h

include(quicktemplates2.pri)
load(qt_module)
