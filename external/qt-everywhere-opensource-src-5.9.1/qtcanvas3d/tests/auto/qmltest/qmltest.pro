TEMPLATE = app

TARGET = tst_qmltest

CONFIG += qmltestcase
CONFIG += console

SOURCES += tst_qmltest.cpp

OTHER_FILES += canvas3d/tst_*.qml canvas3d/*.js canvas3d/*.png ../../../examples/3rdparty/*.js
DEFINES += QUICK_TEST_SOURCE_DIR=\"\\\"$$PWD/canvas3d\\\"\"
