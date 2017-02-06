TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += \
    gamepad \
    plugins

qtHaveModule(quick) {
    src_imports.subdir = imports
    src_imports.depends = gamepad
    SUBDIRS += src_imports
}
