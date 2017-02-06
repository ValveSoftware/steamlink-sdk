TARGET = QtDesignerComponents
MODULE = designercomponents

QT = core gui-private widgets-private designer-private
QT_PRIVATE = xml
CONFIG += internal_module

# QtDesignerComponents uses
DEFINES += QT_STATICPLUGIN
DEFINES += QDESIGNER_COMPONENTS_LIBRARY

static:DEFINES += QT_DESIGNER_STATIC

# Override qt_targets
QMAKE_TARGET_PRODUCT = Designer
QMAKE_TARGET_DESCRIPTION = Graphical user interface designer.

SOURCES += qdesigner_components.cpp

INCLUDEPATH += . .. \
    $$PWD/../../lib/components \
    $$PWD/../../lib/sdk \
    $$PWD/../../lib/extension \
    $$PWD/../../lib/shared

include(../propertyeditor/propertyeditor.pri)
include(../objectinspector/objectinspector.pri)
include(../signalsloteditor/signalsloteditor.pri)
include(../formeditor/formeditor.pri)
include(../widgetbox/widgetbox.pri)
include(../buddyeditor/buddyeditor.pri)
include(../taskmenu/taskmenu.pri)
include(../tabordereditor/tabordereditor.pri)

PRECOMPILED_HEADER= lib_pch.h

# MinGW GCC cc1plus crashes when using precompiled header
# Date Checked: 3rd September 2012
#
# Compilers known to be affected:
# * MinGW-builds GCC 4.6.3 64-bit
# * MinGW-builds GCC 4.7.1 64-bit
# * MinGW-w64 GCC 4.7.1 64-bit (rubenvb)
mingw:CONFIG -= precompile_header

load(qt_module)
