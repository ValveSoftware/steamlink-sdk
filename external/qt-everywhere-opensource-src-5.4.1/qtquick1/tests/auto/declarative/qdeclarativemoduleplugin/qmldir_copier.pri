COPY_IN_FILE = $${_PRO_FILE_PWD_}/qmldir
COPY_OUT_FILE = $$OUT_PWD/$$DESTDIR/qmldir

COPY_OUT_DIR = $$OUT_PWD/$$DESTDIR
COPY_OUT_DIR ~= s,/,$$QMAKE_DIR_SEP,

copy2build.input = COPY_IN_FILE
copy2build.output = $$COPY_OUT_FILE
!contains(TEMPLATE_PREFIX, vc):copy2build.variable_out = PRE_TARGETDEPS
win32 {
    copy2build.commands = ($$QMAKE_CHK_DIR_EXISTS $$COPY_OUT_DIR $(MKDIR) $$COPY_OUT_DIR) && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
} else {
    copy2build.commands = $(MKDIR) $$COPY_OUT_DIR && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
}
copy2build.name = COPY ${QMAKE_FILE_IN}
copy2build.CONFIG += no_link
# `clean' should leave the build in a runnable state, which means it shouldn't delete qmldir
copy2build.CONFIG += no_clean
QMAKE_EXTRA_COMPILERS += copy2build
