CONFIG += testcase
TARGET = tst_qdeclarativepincharea

QT += testlib declarative declarative-private gui-private widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativepincharea.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
