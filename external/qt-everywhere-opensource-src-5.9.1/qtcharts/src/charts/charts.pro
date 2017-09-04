############################# BUILD CONFIG ######################################

TARGET = QtCharts

QT = core gui widgets
contains(QT_COORD_TYPE, float): DEFINES += QT_QREAL_IS_FLOAT

QMAKE_DOCS = $$PWD/doc/qtcharts.qdocconf

QMAKE_TARGET_PRODUCT = "Qt Charts (Qt $$QT_VERSION)"
QMAKE_TARGET_DESCRIPTION = "Charts component for Qt."

############################# SOURCES ##########################################

SOURCES += \
    $$PWD/chartdataset.cpp \
    $$PWD/chartpresenter.cpp \
    $$PWD/chartthememanager.cpp \
    $$PWD/qchart.cpp \
    $$PWD/qchartview.cpp \
    $$PWD/qabstractseries.cpp \
    $$PWD/chartbackground.cpp \
    $$PWD/chartelement.cpp \
    $$PWD/chartitem.cpp \
    $$PWD/scroller.cpp \
    $$PWD/charttitle.cpp \
    $$PWD/qpolarchart.cpp

contains(QT_CONFIG, opengl): SOURCES += $$PWD/glwidget.cpp

PRIVATE_HEADERS += \
    $$PWD/chartdataset_p.h \
    $$PWD/chartitem_p.h \
    $$PWD/chartpresenter_p.h \
    $$PWD/chartthememanager_p.h \
    $$PWD/chartbackground_p.h \
    $$PWD/chartelement_p.h \
    $$PWD/chartconfig_p.h \
    $$PWD/qchart_p.h \
    $$PWD/qchartview_p.h \
    $$PWD/scroller_p.h \
    $$PWD/qabstractseries_p.h \
    $$PWD/charttitle_p.h \
    $$PWD/charthelpers_p.h

contains(QT_CONFIG, opengl): PRIVATE_HEADERS += $$PWD/glwidget_p.h

PUBLIC_HEADERS += \
    $$PWD/qchart.h \
    $$PWD/qchartglobal.h \
    $$PWD/qabstractseries.h \
    $$PWD/qchartview.h \
    $$PWD/chartsnamespace.h \
    $$PWD/qpolarchart.h

include($$PWD/animations/animations.pri)
include($$PWD/areachart/areachart.pri)
include($$PWD/axis/axis.pri)
include($$PWD/domain/domain.pri)
include($$PWD/barchart/barchart.pri)
include($$PWD/legend/legend.pri)
include($$PWD/linechart/linechart.pri)
include($$PWD/piechart/piechart.pri)
include($$PWD/scatterchart/scatter.pri)
include($$PWD/splinechart/splinechart.pri)
include($$PWD/themes/themes.pri)
include($$PWD/xychart/xychart.pri)
include($$PWD/layout/layout.pri)
include($$PWD/boxplotchart/boxplotchart.pri)
include($$PWD/candlestickchart/candlestickchart.pri)

HEADERS += $$PUBLIC_HEADERS
HEADERS += $$PRIVATE_HEADERS
HEADERS += $$THEMES

OTHER_FILES += doc/qtcharts.qdocconf \
               doc/src/* \
               doc/images/*

msvc {
    # Suppress "conversion from 'size_t' to 'int', possible loss of data" warnings in 64bit
    # builds resulting from usage of str::sort
    QMAKE_CXXFLAGS_WARN_ON += -wd4267
}

win32:!winrt:!wince {
    # ChartThemeSystem uses Windows native API
    LIBS += -luser32
}

load(qt_module)
