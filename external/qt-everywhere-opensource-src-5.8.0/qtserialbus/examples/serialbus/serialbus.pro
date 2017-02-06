TEMPLATE = subdirs

qtHaveModule(widgets) {
    SUBDIRS += can \
               modbus
}
