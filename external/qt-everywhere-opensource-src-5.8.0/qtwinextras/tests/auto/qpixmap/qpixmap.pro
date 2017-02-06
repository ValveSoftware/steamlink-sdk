CONFIG += testcase
TARGET = tst_qpixmap
QT += gui testlib winextras
SOURCES += tst_qpixmap.cpp
win32:!qtHaveModule(opengl)|qtConfig(dynamicgl):LIBS += -lgdi32 -luser32
