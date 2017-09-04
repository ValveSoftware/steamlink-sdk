TARGET = clipper

CONFIG += staticlib exceptions warn_off optimize_full

load(qt_helper_lib)

# workaround for QTBUG-31586
contains(QT_CONFIG, c++11): CONFIG += c++11

gcc {
    QMAKE_CFLAGS_OPTIMIZE_FULL += -ffast-math
    !clang:!intel_icc:!rim_qcc: QMAKE_CXXFLAGS_WARN_ON += -Wno-error=return-type
}

HEADERS += clipper.h
SOURCES += clipper.cpp
