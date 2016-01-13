CONFIG += testcase
QT += testlib
SOURCES += tst_xmlpatternsview.cpp

include (../xmlpatterns.pri)

TARGET = tst_xmlpatternsview

wince*: {
    viewexe.files = $$QT.xmlpatterns.bins/xmlpatternsview.exe
    viewexe.path = .
    DEPLOYMENT += viewexe
}
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
