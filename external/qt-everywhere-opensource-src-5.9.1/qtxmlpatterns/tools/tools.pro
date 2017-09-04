TEMPLATE = subdirs
QT_FOR_CONFIG += xmlpatterns-private

!android|android_app {
    SUBDIRS += xmlpatterns
    qtConfig(xml-schema) {
        SUBDIRS += xmlpatternsvalidator
    }
}
