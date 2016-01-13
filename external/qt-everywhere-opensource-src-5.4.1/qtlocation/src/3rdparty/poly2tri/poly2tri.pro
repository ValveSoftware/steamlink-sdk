TARGET = poly2tri

CONFIG += staticlib

load(qt_helper_lib)

# workaround for QTBUG-31586
contains(QT_CONFIG, c++11): CONFIG += c++11

*-g++* {
    QMAKE_CXXFLAGS += -O3 -ftree-vectorize -ffast-math -funsafe-math-optimizations -Wno-error=return-type
}

SOURCES += common/shapes.cpp \
           sweep/sweep_context.cpp \
           sweep/cdt.cpp \
           sweep/sweep.cpp \
           sweep/advancing_front.cpp
