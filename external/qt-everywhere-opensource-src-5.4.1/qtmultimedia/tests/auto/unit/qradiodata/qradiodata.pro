CONFIG += testcase no_private_qt_headers_warning
TARGET = tst_qradiodata
QT += multimedia-private testlib
SOURCES += tst_qradiodata.cpp

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockradio.pri)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
