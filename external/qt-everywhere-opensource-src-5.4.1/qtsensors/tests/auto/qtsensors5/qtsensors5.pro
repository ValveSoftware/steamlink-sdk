TEMPLATE=app
TARGET=tst_qtsensors5
!no_system_tests:CONFIG += testcase
QT = core testlib gui qml
SOURCES += tst_qtsensors5.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
