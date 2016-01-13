TEMPLATE = subdirs

load(qfeatures)
!android|android_app {
    SUBDIRS += xmlpatterns
    !contains(QT_DISABLED_FEATURES, xmlschema) {
        SUBDIRS += xmlpatternsvalidator
    }
}
