TEMPLATE = subdirs

SUBDIRS += cmake webchannel

qtHaveModule(quick) {
    SUBDIRS += qml
}
