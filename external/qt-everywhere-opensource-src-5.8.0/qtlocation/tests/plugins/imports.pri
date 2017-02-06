!contains(DESTDIR, $$[QT_INSTALL_QML]/$$TARGETPATH) {
    importfiles.files = $$IMPORT_FILES
    importfiles.path = $$DESTDIR
    COPIES += importfiles
}
