TEMPLATE = subdirs

SUBDIRS += bluetooth nfc
android: SUBDIRS += android

contains(QT_CONFIG, private_tests) {
    bluetooth_doc_snippets.subdir = bluetooth/doc/snippets
    bluetooth_doc_snippets.depends = bluetooth

    nfc_doc_snippets.subdir = nfc/doc/snippets
    nfc_doc_snippets.depends = nfc

    SUBDIRS += bluetooth_doc_snippets nfc_doc_snippets
}

qtHaveModule(quick) {
    imports.depends += bluetooth nfc
    SUBDIRS += imports
}

config_bluez:qtHaveModule(dbus) {
    SUBDIRS += tools/sdpscanner
}
