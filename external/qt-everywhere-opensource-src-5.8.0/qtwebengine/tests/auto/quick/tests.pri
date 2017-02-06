TEMPLATE = app

CONFIG += testcase
CONFIG += c++11

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

SOURCES += $${TARGET}.cpp
INCLUDEPATH += \
    $$PWD \
    ../shared

QT += testlib network quick webengine

macx: CONFIG -= app_bundle

# This define is used by some tests to look up resources in the source tree
DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD/\\\"

isQMLTestSupportApiEnabled() {
    DEFINES += ENABLE_QML_TESTSUPPORT_API
}
