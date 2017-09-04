TEMPLATE = subdirs

SUBDIRS += multimedia

include($$OUT_PWD/multimedia/qtmultimedia-config.pri)
QT_FOR_CONFIG += multimedia-private

# Everything else depends on multimedia
src_qgsttools.subdir = gsttools
src_qgsttools.depends = multimedia

src_qtmmwidgets.subdir = multimediawidgets
src_qtmmwidgets.depends = multimedia

src_plugins.subdir = plugins
src_plugins.depends = multimedia


qtHaveModule(quick):qtConfig(opengl) {
    src_qtmultimediaquicktools.subdir = qtmultimediaquicktools
    src_qtmultimediaquicktools.depends = multimedia

    src_imports.subdir = imports
    src_imports.depends = multimedia src_qtmultimediaquicktools

    # For the videonode plugin
    src_plugins.depends += src_qtmultimediaquicktools

    SUBDIRS += \
        src_qtmultimediaquicktools \
        src_imports
}

# Optional bits
qtHaveModule(widgets) {
    SUBDIRS += src_qtmmwidgets

    # If widgets is around, plugins depends on widgets too (imports does not)
    src_plugins.depends += src_qtmmwidgets

    # same with qgsttools
    src_qgsttools.depends += src_qtmmwidgets
}

qtConfig(gstreamer) {
    SUBDIRS += src_qgsttools

    # If gstreamer is present, then plugins should depend on it
    src_plugins.depends += src_qgsttools
}

SUBDIRS += src_plugins

