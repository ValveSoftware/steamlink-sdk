TEMPLATE = app

TARGET = tst_geometryloaders

QT += 3dcore 3dcore-private 3drender 3drender-private testlib 3dextras

CONFIG += testcase

SOURCES += tst_geometryloaders.cpp

RESOURCES += \
    geometryloaders.qrc

