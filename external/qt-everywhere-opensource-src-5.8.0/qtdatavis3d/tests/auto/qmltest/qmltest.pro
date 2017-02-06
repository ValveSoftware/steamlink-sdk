TEMPLATE = app
TARGET = tst_qmltest
CONFIG += qmltestcase
CONFIG += console
SOURCES += tst_qmltest.cpp
OTHER_FILES += bars3d/tst_basic.qml \
               bars3d/tst_bars.qml \
               bars3d/tst_barseries.qml \
               bars3d/tst_proxy.qml \
               scatter3d/tst_basic.qml \
               scatter3d/tst_scatter.qml \
               scatter3d/tst_scatterseries.qml \
               scatter3d/tst_proxy.qml \
               surface3d/tst_basic.qml \
               surface3d/tst_surface.qml \
               surface3d/tst_surfaceseries.qml \
               surface3d/tst_proxy.qml \
               surface3d/tst_heightproxy.qml \
               theme3d/tst_theme.qml \
               theme3d/tst_colorgradient.qml \
               theme3d/tst_themecolor.qml \
               custom3d/tst_customitem.qml \
               custom3d/tst_customlabel.qml \
               custom3d/tst_customvolume.qml \
               scene3d/tst_scene.qml \
               scene3d/tst_camera.qml \
               scene3d/tst_light.qml \
               input3d/tst_input.qml \
               input3d/tst_touch.qml \
               axis3d/tst_category.qml \
               axis3d/tst_value.qml \
               axis3d/tst_logvalue.qml \

RESOURCES += \
    qmltest.qrc
