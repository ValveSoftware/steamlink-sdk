TEMPLATE = subdirs

SUBDIRS += serialbus
serialbus.subdir = serialbus
serialbus.target = sub-serialbus

SUBDIRS += plugins
plugins.subdir = plugins
plugins.target = sub-plugins
plugins.depends = serialbus

SUBDIRS += tools
tools.subdir = tools
tools.target = sub-tools
tools.depends = serialbus plugins

!android:contains(QT_CONFIG, private_tests) {
    SUBDIRS += serialbus_doc_snippets
    serialbus_doc_snippets.subdir = serialbus/doc/snippets

    #plugin dependency required during static builds
    serialbus_doc_snippets.depends = serialbus plugins
}
