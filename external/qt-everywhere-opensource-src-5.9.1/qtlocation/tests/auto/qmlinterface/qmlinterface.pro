QT       += location qml testlib

#QT       -= gui

TARGET = tst_qmlinterface
CONFIG   += testcase
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += tst_qmlinterface.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

OTHER_FILES += \
    data/TestCategory.qml \
    data/TestAddress.qml \
    data/TestLocation.qml \
    data/TestPlace.qml \
    data/TestIcon.qml \
    data/TestRatings.qml \
    data/TestSupplier.qml \
    data/TestUser.qml \
    data/TestPlaceAttribute.qml \
    data/TestContactDetail.qml

