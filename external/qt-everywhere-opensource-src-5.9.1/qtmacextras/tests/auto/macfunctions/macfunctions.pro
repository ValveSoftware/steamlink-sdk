CONFIG += testcase console
CONFIG -= app_bundle
TARGET = tst_qmacfunctions
QT += macextras widgets testlib
OBJECTIVE_SOURCES += tst_qmacfunctions.mm
macos: LIBS += -framework AppKit
