CXX_MODULE = location
TARGET  = declarative_location_test
TARGETPATH = QtLocation/Test

QT += gui-private qml quick location testlib

INCLUDEPATH += ../../../src/imports/location
INCLUDEPATH += ../../../src/location

HEADERS += \
    qdeclarativepinchgenerator_p.h \
    qdeclarativelocationtestmodel_p.h \
    testhelper.h

SOURCES += \
    locationtest.cpp \
    qdeclarativepinchgenerator.cpp \
    qdeclarativelocationtestmodel.cpp

IMPORT_FILES = \
    qmldir

load(qml_plugin)


# must be after load(qml_plugin)
include(../imports.pri)
