TARGET = QtAndroidExtras
DEFINES += QT_NO_USING_NAMESPACE
QMAKE_DOCS = \
             $$PWD/doc/qtandroidextras.qdocconf
QT -= gui
QT += core-private

load(qt_module)
include(jni/jni.pri)
include(android/android.pri)
