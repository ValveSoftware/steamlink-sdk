TARGET = tst_test
CONFIG += testcase

QT = core script testlib
SOURCES = ../tst_qscriptextensionplugin.cpp
CONFIG -= app_bundle
LIBS += -L../plugins/script -lstaticplugin

TARGET = tst_qscriptextensionplugin
CONFIG(debug_and_release) {
  CONFIG(debug, debug|release) {
    DESTDIR = ../debug
  } else {
    DESTDIR = ../release
  }
} else {
  DESTDIR = ..
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
