!include( ../auto.pri ) {
    error( "Couldn't find the auto.pri file!" )
}

TEMPLATE = app
CONFIG += warn_on qmltestcase
CONFIG -= testcase
SOURCES += main.cpp
OTHER_FILES += tst_*.qml
DEFINES += QUICK_TEST_SOURCE_DIR=\"\\\"$$PWD\\\"\"
