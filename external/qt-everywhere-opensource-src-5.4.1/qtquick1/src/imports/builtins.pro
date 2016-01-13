TEMPLATE = aux

qmltypes_file = $$_PRO_FILE_PWD_/builtins.qmltypes
exists($$[QT_HOST_PREFIX]/.qmake.cache) {
    # These bizarre rules copy the file to the qtbase build directory

    builtins2build.input = qmltypes_file
    builtins2build.output = $$[QT_INSTALL_IMPORTS]/builtins.qmltypes
    !contains(TEMPLATE, vc.*): builtins2build.variable_out = PRE_TARGETDEPS
    builtins2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
    builtins2build.name = COPY ${QMAKE_FILE_IN}
    builtins2build.CONFIG = no_link no_clean

    QMAKE_EXTRA_COMPILERS += builtins2build
}

# Install rules
builtins.base = $$_PRO_FILE_PWD_
builtins.files = $$qmltypes_file
builtins.path = $$[QT_INSTALL_IMPORTS]
INSTALLS += builtins
