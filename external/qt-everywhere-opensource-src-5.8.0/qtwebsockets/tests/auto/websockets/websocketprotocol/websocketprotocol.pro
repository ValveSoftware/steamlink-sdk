CONFIG += console
CONFIG += testcase
CONFIG -= app_bundle

TEMPLATE = app

TARGET = tst_websocketprotocol

QT = core testlib websockets websockets-private

SOURCES += tst_websocketprotocol.cpp

requires(qtConfig(private_tests))
