TEMPLATE = subdirs
SUBDIRS += xquery
qtHaveModule(widgets) {
    SUBDIRS += recipes

    load(qfeatures)
    !contains(QT_DISABLED_FEATURES, xmlschema): SUBDIRS += filetree schema
}

EXAMPLE_FILES = \
    shared

