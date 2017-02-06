TEMPLATE = subdirs
SUBDIRS += \
           areachart \
           barchart \
           barmodelmapper \
           boxplotchart \
           callout \
           candlestickchart \
           chartinteractions \
           chartthemes \
           customchart \
           donutbreakdown \
           donutchart \
           dynamicspline \
           horizontalbarchart \
           horizontalpercentbarchart \
           horizontalstackedbarchart \
           legend \
           legendmarkers \
           lineandbar \
           linechart \
           logvalueaxis \
           modeldata \
           multiaxis \
           nesteddonuts \
           percentbarchart \
           piechart \
           piechartcustomization \
           piechartdrilldown \
           polarchart \
           scatterchart \
           scatterinteractions \
           splinechart \
           stackedbarchart \
           stackedbarchartdrilldown \
           temperaturerecords \
           zoomlinechart

qtHaveModule(quick) {
    SUBDIRS += \
           qmlaxes \
           qmlboxplot \
           qmlcandlestick \
           qmlchart \
           qmlcustomizations \
           qmlcustomlegend \
           qmlf1legends \
           qmloscilloscope \
           qmlpiechart \
           qmlpolarchart \
           qmlweather

}

qtHaveModule(multimedia) {
    SUBDIRS += audio
} else {
    message("QtMultimedia library not available. Some examples are disabled.")
}

contains(QT_CONFIG, opengl) {
    SUBDIRS += openglseries
} else {
    message("OpenGL not available. Some examples are disabled.")
}

!contains(QT_COORD_TYPE, float): {
SUBDIRS += \
    datetimeaxis
}

