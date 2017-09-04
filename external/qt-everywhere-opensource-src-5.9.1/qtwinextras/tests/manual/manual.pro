TEMPLATE = subdirs
qtHaveModule(widgets) {
    SUBDIRS += \
        dwmfeatures \
        jumplist \
        thumbnail
}
