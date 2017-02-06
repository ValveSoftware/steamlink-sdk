TEMPLATE = app

CONFIG += testcase
CONFIG += c++11

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

SOURCES += $${TARGET}.cpp
INCLUDEPATH += $$PWD

exists($$_PRO_FILE_PWD_/$${TARGET}.qrc): RESOURCES += $${TARGET}.qrc

QT += testlib network webenginewidgets widgets
osx: CONFIG -= app_bundle
