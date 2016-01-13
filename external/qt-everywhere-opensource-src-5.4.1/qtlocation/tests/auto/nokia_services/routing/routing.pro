TEMPLATE = app
CONFIG += testcase
TARGET = tst_nokia_routing

QT += network location testlib
INCLUDEPATH += $$PWD/../../../../src/plugins/geoservices/nokia

HEADERS += $$PWD/../../../../src/plugins/geoservices/nokia/qgeonetworkaccessmanager.h
SOURCES += tst_routing.cpp

OTHER_FILES += *.xml

TESTDATA = $$OTHER_FILES
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
