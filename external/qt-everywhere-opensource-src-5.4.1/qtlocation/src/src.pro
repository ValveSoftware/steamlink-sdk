TEMPLATE = subdirs

SUBDIRS += positioning

plugins.depends = positioning
SUBDIRS += plugins

contains(QT_CONFIG, private_tests) {
    positioning_doc_snippets.subdir = positioning/doc/snippets
    #plugin dependency required during static builds
    positioning_doc_snippets.depends = positioning plugins

    SUBDIRS += positioning_doc_snippets
}

qtHaveModule(quick) {
    SUBDIRS += 3rdparty

    location.depends = positioning 3rdparty
    SUBDIRS += location

    plugins.depends += location
    imports.depends += location

    contains(QT_CONFIG, private_tests) {
        location_doc_snippets.subdir = location/doc/snippets
        #plugin dependency required during static builds
        location_doc_snippets.depends = location plugins

        SUBDIRS += location_doc_snippets
    }

    imports.depends += positioning
    SUBDIRS += imports
}
