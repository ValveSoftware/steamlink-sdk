TEMPLATE = subdirs

SUBDIRS += cmake websockets

qtHaveModule(quick) {
    SUBDIRS += qml
}
