# QML tests in this directory must not depend on an OpenGL context.
# QML tests that do require an OpenGL context must go in ../../declarative_ui.

TEMPLATE = app
TARGET = tst_declarative_core
CONFIG += qmltestcase
SOURCES += main.cpp

QT += location quick

OTHER_FILES = *.qml *.js
TESTDATA = $$OTHER_FILES
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
