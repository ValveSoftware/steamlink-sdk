TEMPLATE = subdirs
SUBDIRS += \
    presenterchart \
    polarcharttest \
    boxplottester \
    candlesticktester

contains(QT_CONFIG, opengl) {
    SUBDIRS +=  chartwidgettest \
                wavechart \
                chartviewer \
                openglseriestest
} else {
    message("OpenGL not available. Some test apps are disabled")
}

qtHaveModule(quick) {
    SUBDIRS += qmlchartproperties \
               qmlchartaxis
}
