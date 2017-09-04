CONFIG += plugin

TEMPLATE = lib

TARGET = $$qtLibraryTarget(qtchartsdesigner)

QT += charts
QT += designer

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
INCLUDEPATH += ../../../include

HEADERS = $$PWD/qchartsplugin.h
SOURCES = $$PWD/qchartsplugin.cpp
RESOURCES = $$PWD/qchartsplugin.qrc

target.path = $$[QT_INSTALL_PLUGINS]/designer
INSTALLS += target
