TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += charts
qtHaveModule(quick) {
    SUBDIRS += chartsqml2
}
