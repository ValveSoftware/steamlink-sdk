TEMPLATE=subdirs
SUBDIRS=cmake

qtHaveModule(widgets) {
    SUBDIRS += qx11info
}