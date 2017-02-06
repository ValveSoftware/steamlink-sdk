TEMPLATE = subdirs

SUBDIRS += simple

qtHaveModule(quick) {
    SUBDIRS += configureButtons

    qtHaveModule(widgets) {
        SUBDIRS += quickGamepad \
                   keyNavigation \
                   mouseItem
    }
}
