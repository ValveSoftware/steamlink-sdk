TARGET = tst_qscriptvalue
CONFIG += testcase
QT = core gui widgets script testlib
SOURCES  += tst_qscriptvalue.cpp
HEADERS  += tst_qscriptvalue.h

win32-msvc* {
    # With -O2, MSVC takes up to 24 minutes to compile this test!
    QMAKE_CXXFLAGS_RELEASE -= -O1 -O2
    QMAKE_CXXFLAGS_RELEASE += -Od
}
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
