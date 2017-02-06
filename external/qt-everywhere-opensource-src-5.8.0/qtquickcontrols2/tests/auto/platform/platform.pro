TEMPLATE = app
TARGET = tst_platform
CONFIG += qmltestcase

SOURCES += \
    $$PWD/tst_platform.cpp

OTHER_FILES += \
    $$PWD/data/*.qml

TESTDATA += \
    $$PWD/data/tst_*
