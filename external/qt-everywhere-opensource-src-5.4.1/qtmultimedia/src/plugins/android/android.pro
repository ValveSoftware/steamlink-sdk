TEMPLATE = subdirs

SUBDIRS += src
android:!android-no-sdk: SUBDIRS += jar

qtHaveModule(quick) {
    SUBDIRS += videonode
}
