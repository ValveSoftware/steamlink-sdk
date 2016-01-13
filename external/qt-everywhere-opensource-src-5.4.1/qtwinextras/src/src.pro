win32 {
    TEMPLATE = subdirs
    CONFIG += ordered
    SUBDIRS += winextras
    qtHaveModule(quick): SUBDIRS += imports
} else {
    # fake project for creating the documentation
    TEMPLATE = aux
    QMAKE_DOCS = $$PWD/winextras/doc/qtwinextras.qdocconf
}
