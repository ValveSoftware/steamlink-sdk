TEMPLATE = subdirs

SUBDIRS += simple

qtHaveModule(quick) {
    SUBDIRS += \
        mouseItem  \
        keyNavigation

    qtHaveModule(quickcontrols2) {
        SUBDIRS += \
            configureButtons \
            quickGamepad
    }
}
