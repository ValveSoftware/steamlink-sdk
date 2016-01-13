TEMPLATE = app

# FIXME: Re-enable once we want to run tests on the CI
# CONFIG += testcase

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

# Load mobilityconfig if Qt Mobility is available
load(mobilityconfig, true)
contains(MOBILITY_CONFIG, multimedia) {
    # This define is used by tests depending on Qt Multimedia
    DEFINES -= WTF_USE_QT_MULTIMEDIA=0
    DEFINES += WTF_USE_QT_MULTIMEDIA=1
}

SOURCES += $${TARGET}.cpp
INCLUDEPATH += $$PWD

QT += testlib network webenginewidgets widgets
macx: CONFIG -= app_bundle

# This define is used by some tests to look up resources in the source tree
DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD/\\\"
