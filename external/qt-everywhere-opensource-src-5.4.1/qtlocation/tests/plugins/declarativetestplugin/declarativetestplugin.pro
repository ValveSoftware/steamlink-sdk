CXX_MODULE = location
TARGET  = declarative_location_test
TARGETPATH = QtLocation/test

QT += gui-private qml quick location testlib

INCLUDEPATH += ../../../src/imports/location
INCLUDEPATH += ../../../src/location

HEADERS += qdeclarativepinchgenerator_p.h \
           qdeclarativelocationtestmodel_p.h


SOURCES += locationtest.cpp \
           qdeclarativepinchgenerator.cpp \
           qdeclarativelocationtestmodel.cpp

IMPORT_FILES = \
    qmldir

load(qml_plugin)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

# must be after load(qml_plugin)
include(../imports.pri)
