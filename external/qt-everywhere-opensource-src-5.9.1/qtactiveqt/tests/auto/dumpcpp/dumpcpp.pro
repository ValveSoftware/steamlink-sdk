CONFIG += testcase
QT += widgets axcontainer testlib
SOURCES += tst_dumpcpp.cpp
TARGET = tst_dumpcpp

# Assume Web Browser type library is available in all windows installations
TYPELIBS = $$(SystemRoot)\\system32\\ieframe.dll

!exists($$TYPELIBS) {
    message("Web Browser type library for test not found!")
}
