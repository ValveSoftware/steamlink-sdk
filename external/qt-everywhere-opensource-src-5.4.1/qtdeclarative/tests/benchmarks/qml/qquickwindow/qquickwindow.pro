CONFIG += testcase
TARGET = tst_qquickwindow
SOURCES += tst_qquickwindow.cpp
macx:CONFIG -= app_bundle

testDataFiles.files = data
testDataFiles.path = .
DEPLOYMENT += testDataFiles

QT += core-private gui-private  qml-private quick-private opengl-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0