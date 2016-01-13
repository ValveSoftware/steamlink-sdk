TEMPLATE = subdirs

SUBDIRS = quick

qtHaveModule(webenginewidgets) {
    SUBDIRS += widgets
}
