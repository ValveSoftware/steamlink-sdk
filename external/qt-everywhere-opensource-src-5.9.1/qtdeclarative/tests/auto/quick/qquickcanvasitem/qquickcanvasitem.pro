QT += core-private gui-private qml-private
TEMPLATE=app
TARGET=tst_qquickcanvasitem

CONFIG += qmltestcase
SOURCES += tst_qquickcanvasitem.cpp

TESTDATA = data/*

OTHER_FILES += \
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
    data/tst_composite.qml \
    data/tst_canvas.qml \
    data/tst_pixel.qml \
    data/tst_gradient.qml \
    data/tst_arcto.qml \
    data/tst_arc.qml \
    data/tst_context.qml \
    data/tst_imagedata.qml \
    data/CanvasTestCase.qml \
    data/CanvasComponent.qml \
    data/tst_image.qml \
    data/tst_svgpath.qml \
    data/anim-gr.gif \
    data/anim-gr.png \
    data/anim-poster-gr.png \
    data/background.png \
    data/broken.png \
    data/ggrr-256x256.png \
    data/green-1x1.png \
    data/green-2x2.png \
    data/green-16x16.png \
    data/green-256x256.png \
    data/green.png \
    data/grgr-256x256.png \
    data/red-16x16.png \
    data/red.png \
    data/redtransparent.png \
    data/rgrg-256x256.png \
    data/rrgg-256x256.png \
    data/transparent.png \
    data/transparent50.png \
    data/yellow.png \
    data/yellow75.png


CONFIG += insignificant_test # QTBUG-41043
