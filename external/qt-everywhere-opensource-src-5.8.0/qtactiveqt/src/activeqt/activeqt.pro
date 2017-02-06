win32:!winrt:!wince {
    TEMPLATE = subdirs

    CONFIG += ordered
    axshared.file = axshared.prx
    SUBDIRS = axshared container control
} else {
    # fake project for creating the documentation
    message("ActiveQt is a Windows Desktop-only module. Will just generate a docs target.")
    TEMPLATE = aux
    QMAKE_DOCS = $$PWD/doc/activeqt.qdocconf
}

