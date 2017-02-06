TEMPLATE = subdirs

qtHaveModule(quick):contains(QT_CONFIG, opengl) {
    SUBDIRS += qtcanvas3d
}
