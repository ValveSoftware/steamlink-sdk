QT += testlib core-private webchannel-private
TEMPLATE = app
TARGET = qml

CONFIG += warn_on qmltestcase

# TODO: running tests without requiring make install
IMPORTPATH += $$OUT_PWD/../../../qml $$PWD

SOURCES += \
    qml.cpp \
    testtransport.cpp \
    testwebchannel.cpp \
    testobject.cpp

HEADERS += \
    testtransport.h \
    testwebchannel.h \
    testobject.h

OTHER_FILES += \
    Client.qml \
    WebChannelTest.qml \
    tst_webchannel.qml \
    tst_metaobjectpublisher.qml \
    tst_bench.qml \
    tst_multiclient.qml

TESTDATA = data/*

DISTFILES += \
    tst_webchannelseparation.qml
