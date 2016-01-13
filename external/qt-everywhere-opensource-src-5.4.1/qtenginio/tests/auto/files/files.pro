QT       += testlib enginio enginio-private core-private

TARGET = tst_files
CONFIG   += console testcase
CONFIG   -= app_bundle

include(../common/common.pri)

TEMPLATE = app

SOURCES += tst_files.cpp
RESOURCES += ../common/testfiles.qrc
