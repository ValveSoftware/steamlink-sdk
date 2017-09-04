TEMPLATE = app

TARGET = tst_gltfplugins

QT += 3dcore 3dcore-private 3drender 3drender-private testlib 3dextras

CONFIG += testcase

SOURCES += tst_gltfplugins.cpp

RESOURCES += \
    images.qrc
