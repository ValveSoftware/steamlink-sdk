CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_qscriptclass_bytearray

SOURCES += tst_qscriptclass_bytearray.cpp
RESOURCES += qscriptclass_bytearray.qrc

include(../../../../examples/script/customclass/bytearrayclass.pri)

QT = core script testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
