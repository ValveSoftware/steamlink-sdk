mac {
    TEMPLATE = subdirs
    SUBDIRS += macextras
}
else {
    # fake project for creating the documentation
    TEMPLATE = aux
    QMAKE_DOCS = $$PWD/macextras/doc/qtmacextras.qdocconf
}
