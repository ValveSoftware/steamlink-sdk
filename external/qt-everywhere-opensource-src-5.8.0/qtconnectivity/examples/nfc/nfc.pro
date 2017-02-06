TEMPLATE = subdirs
qtHaveModule(widgets) {
    SUBDIRS += \
        annotatedurl \
        ndefeditor
}
qtHaveModule(quick) {
    SUBDIRS += \
        poster
}

qtHaveModule(quick): SUBDIRS += corkboard
