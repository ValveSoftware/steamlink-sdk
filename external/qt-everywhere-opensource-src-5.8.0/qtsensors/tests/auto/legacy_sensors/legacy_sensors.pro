TEMPLATE=app
TARGET=tst_legacy_sensors
!no_system_tests:CONFIG += testcase
QT = core testlib gui qml sensors
SOURCES += tst_legacy_sensors.cpp

VPATH += ../qsensor
INCLUDEPATH += ../qsensor

HEADERS += \
    test_backends.h

SOURCES += \
    test_backends.cpp

