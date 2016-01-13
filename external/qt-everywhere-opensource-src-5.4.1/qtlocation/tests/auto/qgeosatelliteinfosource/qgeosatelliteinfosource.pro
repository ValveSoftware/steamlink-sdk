TEMPLATE = app
!no_system_tests:CONFIG += testcase
TARGET=tst_qgeosatelliteinfosource

SOURCES += tst_qgeosatelliteinfosource.cpp \
            testqgeosatelliteinfosource.cpp \
            ../utils/qlocationtestutils.cpp

HEADERS += testqgeosatelliteinfosource_p.h \
            ../utils/qlocationtestutils_p.h

QT += positioning testlib

# Define whether a satellite source is available.  This must match the logic in
# src/location/location.pro or the test will fail on some platforms.
qtHaveModule(simulator):DEFINES += SATELLITE_SOURCE_AVAILABLE
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
