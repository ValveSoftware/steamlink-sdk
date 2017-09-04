QT = core bluetooth testlib

# Android requires GUI application when running test
android: QT += widgets

TARGET = tst_qlowenergycontroller
CONFIG += testcase

SOURCES += tst_qlowenergycontroller.cpp

osx|ios {
    QT += widgets
}
