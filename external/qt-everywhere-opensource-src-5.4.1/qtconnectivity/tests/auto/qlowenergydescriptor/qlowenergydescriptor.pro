SOURCES += tst_qlowenergydescriptor.cpp
TARGET = tst_qlowenergydescriptor
CONFIG += testcase

QT = core bluetooth testlib
blackberry {
    LIBS += -lbtapi
}
