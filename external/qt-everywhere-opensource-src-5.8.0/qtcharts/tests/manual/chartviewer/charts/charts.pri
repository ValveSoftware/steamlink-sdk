INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
SOURCES += \
    $$PWD/font/font.cpp \
    $$PWD/xyseries/linechart.cpp \
    $$PWD/xyseries/scatterchart.cpp \
    $$PWD/xyseries/splinechart.cpp \
    $$PWD/xyseries/areachart.cpp \
    $$PWD/barseries/verticalstackedbarchart.cpp \
    $$PWD/barseries/horizontalstackedbarchart.cpp \
    $$PWD/barseries/verticalbarchart.cpp \
    $$PWD/barseries/horizontalbarchart.cpp \
    $$PWD/barseries/horizontalpercentbarchart.cpp \
    $$PWD/barseries/verticalpercentbarchart.cpp \
    $$PWD/pieseries/piechart.cpp \
    $$PWD/pieseries/donutchart.cpp \
    $$PWD/axis/valueaxis.cpp \
    $$PWD/axis/logvalueaxisx.cpp \
    $$PWD/axis/logvalueaxisy.cpp \
    $$PWD/axis/categoryaxis.cpp \
    $$PWD/axis/barcategoryaxisx.cpp \
    $$PWD/axis/barcategoryaxisy.cpp \
    $$PWD/axis/barcategoryaxisxlogy.cpp \
    $$PWD/axis/barcategoryaxisylogx.cpp \
    $$PWD/multiaxis/multivalueaxis.cpp \
    $$PWD/multiaxis/multivalueaxis2.cpp \
    $$PWD/multiaxis/multivalueaxis3.cpp \
    $$PWD/multiaxis/multivalueaxis4.cpp \
    $$PWD/multiaxis/multivaluebaraxis.cpp \
    $$PWD/size/sizecharts.cpp \
    $$PWD/domain/barlogy.cpp \
    $$PWD/domain/barlogx.cpp \
    $$PWD/domain/barstackedlogy.cpp \
    $$PWD/domain/barstackedlogx.cpp \
    $$PWD/domain/barpercentlogy.cpp \
    $$PWD/domain/barpercentlogx.cpp \
    $$PWD/domain/linelogxlogy.cpp \
    $$PWD/domain/linelogxy.cpp \
    $$PWD/domain/linexlogy.cpp \
    $$PWD/domain/splinelogxlogy.cpp \
    $$PWD/domain/splinelogxy.cpp \
    $$PWD/domain/splinexlogy.cpp \
    $$PWD/domain/scatterlogxlogy.cpp \
    $$PWD/domain/scatterlogxy.cpp \
    $$PWD/domain/scatterxlogy.cpp

!contains(QT_COORD_TYPE, float): {
SOURCES += \
    $$PWD/axis/datetimeaxisx.cpp \
    $$PWD/axis/datetimeaxisy.cpp
}
