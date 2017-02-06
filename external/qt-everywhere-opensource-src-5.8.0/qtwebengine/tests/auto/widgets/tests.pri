TEMPLATE = app

CONFIG += testcase
CONFIG += c++11

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

SOURCES += $${TARGET}.cpp
INCLUDEPATH += $$PWD

RESOURCES += ../resources/tests.qrc
exists($$_PRO_FILE_PWD_/$${TARGET}.qrc): RESOURCES += $${TARGET}.qrc

QT += testlib network webenginewidgets widgets
macx: CONFIG -= app_bundle

# This define is used by some tests to look up resources in the source tree
DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD/\\\"
