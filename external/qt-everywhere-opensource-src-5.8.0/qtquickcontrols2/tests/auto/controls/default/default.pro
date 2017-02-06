TEMPLATE = app
TARGET = tst_default
CONFIG += qmltestcase

DEFINES += TST_CONTROLS_DATA=\\\"$$QQC2_SOURCE_TREE/tests/auto/controls/data\\\"

SOURCES += \
    $$PWD/tst_default.cpp

OTHER_FILES += \
    $$PWD/../data/*.qml

TESTDATA += \
    $$PWD/../data/tst_*
