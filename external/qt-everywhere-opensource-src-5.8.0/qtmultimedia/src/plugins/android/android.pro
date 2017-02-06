TEMPLATE = subdirs

SUBDIRS += src
android: SUBDIRS += jar

qtHaveModule(quick) {
    SUBDIRS += videonode
}
