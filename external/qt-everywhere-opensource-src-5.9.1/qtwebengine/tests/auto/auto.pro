TEMPLATE = subdirs

SUBDIRS = quick

qtHaveModule(webenginewidgets) {
    SUBDIRS += widgets
# core tests depend on widgets for now
    SUBDIRS += core
}
