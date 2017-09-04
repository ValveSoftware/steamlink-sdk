include(../tests.pri)

DISTFILES += \
    dict/en-US.dic \
    dict/en-US.aff \
    dict/de-DE.dic \
    dict/de-DE.aff \

qtPrepareTool(CONVERT_TOOL, qwebengine_convert_dict)

debug_and_release {
    CONFIG(debug, debug|release): DICTIONARIES_DIR = debug/qtwebengine_dictionaries
    else: DICTIONARIES_DIR = release/qtwebengine_dictionaries
} else {
    DICTIONARIES_DIR = qtwebengine_dictionaries
}

dict.files = $$PWD/dict/en-US.dic $$PWD/dict/de-DE.dic
dictoolbuild.input = dict.files
dictoolbuild.output = $${DICTIONARIES_DIR}/${QMAKE_FILE_BASE}.bdic
dictoolbuild.commands = $${CONVERT_TOOL} ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
dictoolbuild.name = Build ${QMAKE_FILE_IN_BASE}
dictoolbuild.CONFIG = no_link target_predeps
QMAKE_EXTRA_COMPILERS += dictoolbuild
