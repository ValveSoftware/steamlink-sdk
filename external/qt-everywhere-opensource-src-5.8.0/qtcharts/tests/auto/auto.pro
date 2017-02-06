TEMPLATE = subdirs
SUBDIRS += \
           qchartview \
           qchart \
           qlineseries \ 
           qbarset \
           qbarseries \
           qstackedbarseries \
           qpercentbarseries \
           qpieslice qpieseries \
           qpiemodelmapper \
           qsplineseries \
           qscatterseries \
           qxymodelmapper \
           qbarmodelmapper \
           qhorizontalbarseries \
           qhorizontalstackedbarseries \
           qhorizontalpercentbarseries \
           qvalueaxis \
           qlogvalueaxis \
           qcategoryaxis \
           qbarcategoryaxis \
           domain \
           chartdataset \
           qlegend \
           qareaseries \
           cmake \
           qcandlestickmodelmapper \
           qcandlestickseries \
           qcandlestickset

!contains(QT_COORD_TYPE, float): {
    SUBDIRS += \
           qdatetimeaxis
}

qtHaveModule(quick) {
    SUBDIRS += qml \
               qml-qtquicktest
}

!contains(QT_CONFIG, private_tests): SUBDIRS -= \
    domain \
    chartdataset

