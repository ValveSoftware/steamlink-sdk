CONFIG += testcase
TARGET = tst_modelview
QT += testlib remoteobjects
#QT -= gui

SOURCES += $$PWD/tst_modelview.cpp $$PWD/modeltest.cpp
HEADERS += $$PWD/modeltest.h

contains(QT_CONFIG, c++11): CONFIG += c++11

