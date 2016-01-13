#-------------------------------------------------
#
# Project created by QtCreator 2011-11-09T15:45:51
#
#-------------------------------------------------

QT       += location qml testlib

#QT       -= gui

TARGET = tst_qmlinterface
CONFIG   += console
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

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
