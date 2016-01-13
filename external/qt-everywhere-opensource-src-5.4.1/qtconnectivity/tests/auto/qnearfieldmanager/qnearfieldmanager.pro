requires(contains(QT_CONFIG, private_tests))

SOURCES += tst_qnearfieldmanager.cpp
TARGET = tst_qnearfieldmanager
CONFIG += testcase

QT = core nfc-private testlib

INCLUDEPATH += ../../../src/nfc
VPATH += ../../../src/nfc

HEADERS += \
    qnearfieldmanagervirtualbase_p.h \
    qnearfieldmanager_emulator_p.h \
    qnearfieldtarget_emulator_p.h \
    targetemulator_p.h

SOURCES += \
    qnearfieldmanagervirtualbase.cpp \
    qnearfieldmanager_emulator.cpp \
    qnearfieldtarget_emulator.cpp \
    targetemulator.cpp

DEFINES += SRCDIR=\\\"$$PWD/../nfcdata\\\"
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

