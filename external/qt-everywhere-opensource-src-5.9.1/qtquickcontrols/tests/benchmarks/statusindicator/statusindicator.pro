TEMPLATE = app
TARGET = tst_bench_statusindicator
QT = core quick testlib
CONFIG += benchmark

SOURCES += \
    tst_statusindicator.cpp

OTHER_FILES += \
    LotsOfIndicatorsOn.qml \
    LotsOfIndicatorsOff.qml

# Define SRCDIR equal to test's source directory
DEFINES += SRCDIR=\\\"$$PWD\\\"
