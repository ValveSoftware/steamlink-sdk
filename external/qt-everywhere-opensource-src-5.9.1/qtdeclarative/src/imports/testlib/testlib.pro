CXX_MODULE = qml
TARGET  = qmltestplugin
TARGETPATH = QtTest
IMPORT_VERSION = 1.0

QT += qml quick qmltest qmltest-private  qml-private core-private testlib

SOURCES += main.cpp

QML_FILES = \
    TestCase.qml \
    SignalSpy.qml \
    testlogger.js

load(qml_plugin)

OTHER_FILES += testlib.json
