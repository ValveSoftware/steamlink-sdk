TARGET = tst_qsvggenerator
CONFIG += testcase
QT += svg xml testlib widgets gui-private

SOURCES += tst_qsvggenerator.cpp

wince* {
    addFiles.files = referenceSvgs
    addFiles.path = .
    DEPLOYMENT += addFiles
}

wince* {
    DEFINES += SRCDIR=\\\"\\\"
} else {
    DEFINES += SRCDIR=\\\"$$PWD/\\\"
}
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
