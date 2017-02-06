QT += core-private gui-private qml-private
TEMPLATE=app
TARGET=tst_qquicklayouts

CONFIG += qmltestcase
SOURCES += tst_qquicklayouts.cpp

TESTDATA = data/*

OTHER_FILES += \
    data/tst_rowlayout.qml \
    data/tst_gridlayout.qml

