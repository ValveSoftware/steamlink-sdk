TEMPLATE = app
!no_system_tests:CONFIG += testcase
TARGET=tst_qgeosatelliteinfosource

SOURCES += tst_qgeosatelliteinfosource.cpp \
            testqgeosatelliteinfosource.cpp \
            ../utils/qlocationtestutils.cpp

HEADERS += testqgeosatelliteinfosource_p.h \
            ../utils/qlocationtestutils_p.h

QT += positioning testlib
