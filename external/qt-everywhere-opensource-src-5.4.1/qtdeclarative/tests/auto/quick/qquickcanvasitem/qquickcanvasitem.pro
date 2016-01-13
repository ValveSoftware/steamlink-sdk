QT += core-private gui-private qml-private
TEMPLATE=app
TARGET=tst_qquickcanvasitem

CONFIG += qmltestcase
SOURCES += tst_qquickcanvasitem.cpp

TESTDATA = data/*

OTHER_FILES += \
    data/testhelper.js \
    data/tst_transform.qml \
    data/tst_text.qml \
    data/tst_strokeStyle.qml \
    data/tst_state.qml \
    data/tst_shadow.qml \
    data/tst_pattern.qml \
    data/tst_path.qml \
    data/tst_line.qml \
    data/tst_fillStyle.qml \
    data/tst_fillrect.qml \
    data/tst_drawimage.qml \
    data/tst_composite.qml \
    data/tst_canvas.qml \
    data/tst_pixel.qml \
    data/tst_gradient.qml \
    data/tst_arcto.qml \
    data/tst_arc.qml \
    data/tst_context.qml


DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
CONFIG += insignificant_test # QTBUG-41043
