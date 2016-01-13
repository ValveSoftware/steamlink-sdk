android {
    TEMPLATE = subdirs
    SUBDIRS += androidextras
} else {
    TEMPLATE = aux
    QMAKE_DOCS = $$PWD/androidextras/doc/qtandroidextras.qdocconf
}
