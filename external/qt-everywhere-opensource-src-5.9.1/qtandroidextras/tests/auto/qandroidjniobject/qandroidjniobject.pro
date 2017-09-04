CONFIG += testcase
TARGET = tst_qandroidjniobject
QT += testlib androidextras
SOURCES += tst_qandroidjniobject.cpp

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/testdata
OTHER_FILES += \
              testdata/src/org/qtproject/qt5/android/testdata/QtAndroidJniObjectTestClass.java
