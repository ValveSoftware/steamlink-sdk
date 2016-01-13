TEMPLATE = subdirs

SUBDIRS += enginio_client

qtHaveModule(qml) {
    SUBDIRS += enginio_plugin
    enginio_plugin.depends = enginio_client
}
