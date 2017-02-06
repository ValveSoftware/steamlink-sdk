TEMPLATE = subdirs

qtHaveModule(positioning) {
    SUBDIRS += positioning

    qtHaveModule(location): SUBDIRS += location
}
