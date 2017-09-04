TEMPLATE = lib
TARGET = gruesensor

macos: DESTDIR = ../grue_app.app/Contents/Frameworks
else: DESTDIR = $$OUT_PWD/..

macos: QMAKE_SONAME_PREFIX = @rpath

DEFINES *= QT_BUILD_GRUE_LIB
QT = core sensors

HEADERS += gruesensor.h \
           gruesensor_p.h

SOURCES += gruesensor.cpp

target.path=$$[QT_INSTALL_EXAMPLES]/sensors/grue
INSTALLS += target

CONFIG += install_ok  # Do not cargo-cult this!
