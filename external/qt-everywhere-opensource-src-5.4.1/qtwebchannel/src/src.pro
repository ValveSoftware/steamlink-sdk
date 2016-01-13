TEMPLATE = subdirs

SUBDIRS += webchannel

qtHaveModule(quick) {
    SUBDIRS += imports
    imports.depends = webchannel
}
