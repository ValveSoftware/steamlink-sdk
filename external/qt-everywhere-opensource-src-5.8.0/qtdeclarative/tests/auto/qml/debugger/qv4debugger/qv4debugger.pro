CONFIG += testcase
TARGET = tst_qv4debugger
osx:CONFIG -= app_bundle

SOURCES += \
    $$PWD/tst_qv4debugger.cpp \
    $$PWD/../../../../../src/plugins/qmltooling/qmldbg_debugger/qv4datacollector.cpp \
    $$PWD/../../../../../src/plugins/qmltooling/qmldbg_debugger/qv4debugger.cpp \
    $$PWD/../../../../../src/plugins/qmltooling/qmldbg_debugger/qv4debugjob.cpp

HEADERS += \
    $$PWD/../../../../../src/plugins/qmltooling/qmldbg_debugger/qv4datacollector.h \
    $$PWD/../../../../../src/plugins/qmltooling/qmldbg_debugger/qv4debugger.h \
    $$PWD/../../../../../src/plugins/qmltooling/qmldbg_debugger/qv4debugjob.h

INCLUDEPATH += \
    $$PWD/../../../../../src/plugins/qmltooling/qmldbg_debugger

QT += core-private gui-private qml-private network testlib
