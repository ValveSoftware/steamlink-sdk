TEMPLATE = subdirs
android {
    qtHaveModule(quick) {
        SUBDIRS += notification
        EXAMPLE_FILES += notification
    }
}
