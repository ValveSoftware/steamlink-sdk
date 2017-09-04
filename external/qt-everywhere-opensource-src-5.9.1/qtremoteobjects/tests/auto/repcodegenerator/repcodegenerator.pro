CONFIG += testcase
TARGET = tst_repcodegenerator
QT += testlib remoteobjects
QT -= gui

SOURCES += tst_repcodegenerator.cpp

REP_FILES += \
    classwithsignalonlytest.rep \
    preprocessortest.rep \

# Make sure we test both source + replica generated code
REPC_MERGED = $$REP_FILES

contains(QT_CONFIG, c++11): CONFIG += c++11
