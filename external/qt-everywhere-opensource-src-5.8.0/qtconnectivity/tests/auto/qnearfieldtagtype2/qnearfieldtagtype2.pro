requires(contains(QT_CONFIG, private_tests))

SOURCES += tst_qnearfieldtagtype2.cpp
TARGET = tst_qnearfieldtagtype2
CONFIG += testcase

QT = core nfc-private testlib

INCLUDEPATH += ../../../src/nfc
VPATH += ../../../src/nfc

HEADERS += \
    qnearfieldmanagervirtualbase_p.h \
    qnearfieldtarget_emulator_p.h \
    qnearfieldmanager_emulator_p.h \
    targetemulator_p.h

SOURCES += \
    qnearfieldmanagervirtualbase.cpp \
    qnearfieldtarget_emulator.cpp \
    qnearfieldmanager_emulator.cpp \
    targetemulator.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"
