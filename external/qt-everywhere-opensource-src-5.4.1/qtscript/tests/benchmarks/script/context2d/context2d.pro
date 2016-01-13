CONFIG += testcase
TEMPLATE = app
TARGET = tst_bench_context2d

SOURCES += tst_context2d.cpp

CONTEXT2D_EXAMPLE_DIR = ../../../../examples/script/context2d
INCLUDEPATH += $$CONTEXT2D_EXAMPLE_DIR

HEADERS += $$CONTEXT2D_EXAMPLE_DIR/qcontext2dcanvas.h \
           $$CONTEXT2D_EXAMPLE_DIR/context2d.h \
           $$CONTEXT2D_EXAMPLE_DIR/domimage.h \
           $$CONTEXT2D_EXAMPLE_DIR/environment.h

SOURCES += $$CONTEXT2D_EXAMPLE_DIR/qcontext2dcanvas.cpp \
           $$CONTEXT2D_EXAMPLE_DIR/context2d.cpp \
           $$CONTEXT2D_EXAMPLE_DIR/domimage.cpp \
           $$CONTEXT2D_EXAMPLE_DIR/environment.cpp

RESOURCES += $$CONTEXT2D_EXAMPLE_DIR/context2d.qrc

QT += script testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
