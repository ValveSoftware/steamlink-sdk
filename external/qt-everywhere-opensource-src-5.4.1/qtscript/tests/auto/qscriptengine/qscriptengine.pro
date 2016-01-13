TARGET = tst_qscriptengine
CONFIG += testcase
QT = core gui widgets script script-private testlib
SOURCES += tst_qscriptengine.cpp 
RESOURCES += qscriptengine.qrc
include(../shared/util.pri)

wince* {
    DEFINES += SRCDIR=\\\"./\\\"
} else {
    DEFINES += SRCDIR=\\\"$$PWD\\\"
}

wince* {
   addFiles.files = script
   addFiles.path = .
   DEPLOYMENT += addFiles
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
