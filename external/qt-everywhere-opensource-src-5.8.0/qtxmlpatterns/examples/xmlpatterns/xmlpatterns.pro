TEMPLATE = subdirs
QT_FOR_CONFIG += xmlpatterns-private
SUBDIRS += xquery
qtHaveModule(widgets) {
    SUBDIRS += recipes

    qtConfig(xml-schema): SUBDIRS += filetree schema
}

EXAMPLE_FILES = \
    shared

