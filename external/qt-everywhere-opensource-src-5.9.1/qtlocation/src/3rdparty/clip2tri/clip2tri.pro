TARGET = clip2tri

CONFIG += staticlib exceptions warn_off optimize_full

INCLUDEPATH += ../poly2tri
INCLUDEPATH += ../clipper

load(qt_helper_lib)

# workaround for QTBUG-31586
contains(QT_CONFIG, c++11): CONFIG += c++11

gcc {
    QMAKE_CFLAGS_OPTIMIZE_FULL += -ffast-math
    !clang:!intel_icc:!rim_qcc: QMAKE_CXXFLAGS_WARN_ON += -Wno-error=return-type
}

HEADERS += clip2tri.h
SOURCES += clip2tri.cpp

LIBS_PRIVATE += -L$$MODULE_BASE_OUTDIR/lib -lpoly2tri$$qtPlatformTargetSuffix() -lclipper$$qtPlatformTargetSuffix()

