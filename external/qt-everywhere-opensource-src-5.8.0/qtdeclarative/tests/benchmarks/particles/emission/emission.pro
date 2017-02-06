CONFIG += benchmark
TARGET = tst_emission
SOURCES += tst_emission.cpp
macx:CONFIG -= app_bundle

testDataFiles.files = data
testDataFiles.path = .
DEPLOYMENT += testDataFiles

QT += quickparticles-private testlib
