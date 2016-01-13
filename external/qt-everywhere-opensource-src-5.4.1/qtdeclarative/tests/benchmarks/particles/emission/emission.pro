CONFIG += testcase
TARGET = tst_emission
SOURCES += tst_emission.cpp
macx:CONFIG -= app_bundle

testDataFiles.files = data
testDataFiles.path = .
DEPLOYMENT += testDataFiles

QT += quickparticles-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
