TEMPLATE = subdirs

SUBDIRS += scxml

qtHaveModule(qml) {
    SUBDIRS += imports
    imports.depends = scxml
}
