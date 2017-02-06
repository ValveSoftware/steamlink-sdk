TARGET = QtDataVisualization

QT += core gui
osx: QT +=  gui-private

MODULE_INCNAME = QtDataVisualization

QMAKE_DOCS = $$PWD/doc/qtdatavis3d.qdocconf

QMAKE_TARGET_PRODUCT = "Qt Data Visualization (Qt $$QT_VERSION)"
QMAKE_TARGET_DESCRIPTION = "3D Data Visualization component for Qt."

include($$PWD/global/global.pri)
include($$PWD/engine/engine.pri)
include($$PWD/utils/utils.pri)
include($$PWD/theme/theme.pri)
include($$PWD/axis/axis.pri)
include($$PWD/data/data.pri)
include($$PWD/input/input.pri)

OTHER_FILES += doc/qtdatavis3d.qdocconf \
               doc/src/* \
               doc/images/* \
               doc/snippets/* \
               global/*.qdoc

load(qt_module)
