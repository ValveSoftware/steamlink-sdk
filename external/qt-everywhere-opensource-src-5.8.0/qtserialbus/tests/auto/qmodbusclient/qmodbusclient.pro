requires(contains(QT_CONFIG, private_tests))

QT = core testlib serialbus core-private serialbus-private
TARGET = tst_qmodbusclient
CONFIG += testcase c++11

CONFIG -= app_bundle

SOURCES += tst_qmodbusclient.cpp
