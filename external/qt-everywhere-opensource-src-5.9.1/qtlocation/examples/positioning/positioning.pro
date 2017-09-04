TEMPLATE = subdirs

qtHaveModule(widgets): SUBDIRS += logfilepositionsource
qtHaveModule(quick) {
    SUBDIRS += geoflickr satelliteinfo
    qtHaveModule(network): SUBDIRS += weatherinfo
}
