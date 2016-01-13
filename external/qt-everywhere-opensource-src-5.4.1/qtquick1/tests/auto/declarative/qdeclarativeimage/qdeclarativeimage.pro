CONFIG += testcase
TARGET = tst_qdeclarativeimage

QT += testlib declarative declarative-private gui widgets network
macx:CONFIG -= app_bundle

HEADERS += ../shared/testhttpserver.h
SOURCES += tst_qdeclarativeimage.cpp ../shared/testhttpserver.cpp

DEFINES += SRCDIR=\\\"$$PWD\\\"

linux-*:system(". /etc/lsb-release && [ $DISTRIB_CODENAME = lucid ]"):DEFINES+=UBUNTU_LUCID # QTBUG-26787
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
