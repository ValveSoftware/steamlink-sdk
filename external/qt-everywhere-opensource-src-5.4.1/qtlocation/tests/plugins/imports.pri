!contains(DESTDIR, $$[QT_INSTALL_QML]/$$TARGETPATH) {
    copyimportfiles.input = IMPORT_FILES
    copyimportfiles.output = $$DESTDIR/${QMAKE_FILE_IN_BASE}${QMAKE_FILE_EXT}
    copyimportfiles.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
    copyimportfiles.CONFIG += no_link_no_clean
    copyimportfiles.variable_out = PRE_TARGETDEPS
    QMAKE_EXTRA_COMPILERS += copyimportfiles
}
