TARGET = poly2tri

CONFIG += staticlib warn_off optimize_full

load(qt_helper_lib)

# workaround for QTBUG-31586
contains(QT_CONFIG, c++11): CONFIG += c++11

gcc {
    QMAKE_CFLAGS_OPTIMIZE_FULL += -ffast-math
    !clang:!intel_icc:!rim_qcc: QMAKE_CXXFLAGS_WARN_ON += -Wno-error=return-type
}

HEADERS +=  poly2tri.h \
            common/shapes.h \
            common/utils.h \
            sweep/advancing_front.h \
            sweep/cdt.h \
            sweep/sweep.h \
            sweep/sweep_context.h

SOURCES += common/shapes.cpp \
           sweep/sweep_context.cpp \
           sweep/cdt.cpp \
           sweep/sweep.cpp \
           sweep/advancing_front.cpp
