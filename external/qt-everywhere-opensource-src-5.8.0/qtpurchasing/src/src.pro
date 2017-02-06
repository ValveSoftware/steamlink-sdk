TEMPLATE = subdirs
SUBDIRS = purchasing

qtHaveModule(qml):qtHaveModule(quick) {
    src_imports.subdir = imports
    src_imports.depends = purchasing
    SUBDIRS += src_imports
}

android {
    SUBDIRS += android
}
