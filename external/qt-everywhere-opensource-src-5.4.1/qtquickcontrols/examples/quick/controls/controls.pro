TEMPLATE = subdirs

SUBDIRS += \
    gallery \
    tableview \
    touch \
    basiclayouts \
    styles

qtHaveModule(widgets) {
    SUBDIRS += texteditor
}

qtHaveModule(sql) {
    SUBDIRS += calendar
}
