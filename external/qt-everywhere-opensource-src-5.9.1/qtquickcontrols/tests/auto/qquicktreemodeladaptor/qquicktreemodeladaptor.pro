TEMPLATE = app
TARGET = tst_qquicktreemodeladaptor

CONFIG += testcase
CONFIG -= app_bundle
QT = core testlib

INCLUDEPATH += $$PWD/../../../src/controls/Private
HEADERS += $$PWD/../../../src/controls/Private/qquicktreemodeladaptor_p.h \
           $$PWD/../shared/testmodel.h
SOURCES += $$PWD/tst_qquicktreemodeladaptor.cpp \
           $$PWD/../../../src/controls/Private/qquicktreemodeladaptor.cpp
