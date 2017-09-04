TEMPLATE = app
TARGET = spellchecker
QT += webenginewidgets
CONFIG += c++11

contains(WEBENGINE_CONFIG, use_native_spellchecker) {
    error("Spellcheck example can not be built when using native OS dictionaries.")
}

HEADERS += \
    webview.h

SOURCES += \
    main.cpp \
    webview.cpp

RESOURCES += \
    data/spellchecker.qrc

DISTFILES += \
    dict/en/README.txt \
    dict/en/en-US.dic \
    dict/en/en-US.aff \
    dict/de/README.txt \
    dict/de/de-DE.dic \
    dict/de/de-DE.aff

target.path = $$[QT_INSTALL_EXAMPLES]/webenginewidgets/spellchecker
INSTALLS += target

qtPrepareTool(CONVERT_TOOL, qwebengine_convert_dict)

debug_and_release {
    CONFIG(debug, debug|release): DICTIONARIES_DIR = debug/qtwebengine_dictionaries
    else: DICTIONARIES_DIR = release/qtwebengine_dictionaries
} else {
    DICTIONARIES_DIR = qtwebengine_dictionaries
}

dict_base_paths = en/en-US de/de-DE
for (base_path, dict_base_paths) {
    dict.files += $$PWD/dict/$${base_path}.dic
}

dictoolbuild.input = dict.files
dictoolbuild.output = $${DICTIONARIES_DIR}/${QMAKE_FILE_BASE}.bdic
dictoolbuild.depends = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.aff
dictoolbuild.commands = $${CONVERT_TOOL} ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
dictoolbuild.name = Build ${QMAKE_FILE_IN_BASE}
dictoolbuild.CONFIG = no_link target_predeps
QMAKE_EXTRA_COMPILERS += dictoolbuild

# When the example is compiled as a bundle, WebEngine expects to find the dictionaries in
# bundle.app/Contents/Resources/qtwebengine_dictionaries
macos:app_bundle {
    for (base_path, dict_base_paths) {
        base_path_splitted = $$split(base_path, /)
        base_name = $$last(base_path_splitted)
        binary_dict_files.files += $${DICTIONARIES_DIR}/$${base_name}.bdic
    }
    binary_dict_files.path = Contents/Resources/$$DICTIONARIES_DIR
    QMAKE_BUNDLE_DATA += binary_dict_files
}
