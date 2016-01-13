TEMPLATE = app
CONFIG += testcase
TARGET = tst_qplaces

HEADERS += ../../placemanager_utils/placemanager_utils.h

SOURCES += tst_places.cpp \
           ../../placemanager_utils/placemanager_utils.cpp

QT += location testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
