TEMPLATE = lib
TARGET = gruesensor

DESTDIR = $$OUT_PWD/..

DEFINES *= QT_BUILD_GRUE_LIB
QT = core sensors

HEADERS += gruesensor.h \
           gruesensor_p.h

SOURCES += gruesensor.cpp

target.path=$$[QT_INSTALL_EXAMPLES]/sensors/grue
INSTALLS += target

CONFIG += install_ok  # Do not cargo-cult this!
