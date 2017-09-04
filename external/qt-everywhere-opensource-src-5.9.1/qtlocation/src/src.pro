TEMPLATE = subdirs

SUBDIRS += 3rdparty/clip2tri 3rdparty/clipper 3rdparty/poly2tri
3rdparty/clip2tri.depends = 3rdparty/clipper 3rdparty/poly2tri

SUBDIRS += positioning
positioning.depends = 3rdparty/clip2tri

qtHaveModule(quick) {
    plugins.depends += positioning

    SUBDIRS += location
    location.depends += positioning 3rdparty/clip2tri

    plugins.depends += location

    SUBDIRS += imports
    imports.depends += positioning location
}

SUBDIRS += plugins

!android:contains(QT_CONFIG, private_tests) {
    SUBDIRS += positioning_doc_snippets
    positioning_doc_snippets.subdir = positioning/doc/snippets

    #plugin dependency required during static builds
    positioning_doc_snippets.depends = positioning plugins

    qtHaveModule(quick) {
        SUBDIRS += location_doc_snippets
        location_doc_snippets.subdir = location/doc/snippets

        #plugin dependency required during static builds
        location_doc_snippets.depends = location plugins
    }
}
