TEMPLATE = subdirs
CONFIG += ordered

qtConfig(textcodec) {
    SUBDIRS += websockets
    qtHaveModule(quick): SUBDIRS += imports
}
