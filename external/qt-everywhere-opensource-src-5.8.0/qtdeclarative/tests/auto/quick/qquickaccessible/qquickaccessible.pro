CONFIG += testcase

TARGET = tst_qquickaccessible
QT += qml-private network quick-private testlib gui-private core-private
macx:CONFIG -= app_bundle

SOURCES  += tst_qquickaccessible.cpp

include (../../shared/util.pri)

TESTDATA = data/*

OTHER_FILES += data/checkbuttons.qml \
    data/hittest.qml \
    data/pushbutton.qml \
    data/statictext.qml \
    data/ignored.qml \
