CONFIG += testcase
QT = core testlib

DISTFILES += \
    testdata/good/minimal/qt_attribution.json \
    testdata/good/complete/qt_attribution.json \
    testdata/good/expected.json \
    testdata/good/expected.error \
    testdata/warnings/incomplete/qt_attribution.json \
    testdata/warnings/incomplete/expected.json \
    testdata/warnings/incomplete/expected.error \
    testdata/warnings/unknown/qt_attribution.json \
    testdata/warnings/unknown/expected.json \
    testdata/warnings/unknown/expected.error

TARGET = tst_qtattributionsscanner

SOURCES += tst_qtattributionsscanner.cpp
