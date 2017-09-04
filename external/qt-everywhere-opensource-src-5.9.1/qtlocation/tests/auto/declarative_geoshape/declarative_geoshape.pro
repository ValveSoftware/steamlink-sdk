# QML tests in this directory must not depend on an OpenGL context.

TEMPLATE = app
TARGET = tst_declarative_geoshape
CONFIG += qmltestcase
SOURCES += main.cpp

QT += positioning quick

OTHER_FILES = *.qml
TESTDATA = $$OTHER_FILES
