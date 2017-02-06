TARGET = clip2tri

CONFIG += staticlib exceptions

load(qt_helper_lib)

# workaround for QTBUG-31586
contains(QT_CONFIG, c++11): CONFIG += c++11

*-g++* {
    QMAKE_CXXFLAGS += -O3 -ftree-vectorize -ffast-math -funsafe-math-optimizations -Wno-error=return-type
}

HEADERS += clip2tri.h
SOURCES += clip2tri.cpp

LIBS_PRIVATE += -L$$MODULE_BASE_OUTDIR/lib -lpoly2tri$$qtPlatformTargetSuffix() -lclipper$$qtPlatformTargetSuffix()

