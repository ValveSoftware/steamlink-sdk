TARGETPATH = QtQml
QML_FILES += plugins.qmltypes

load(qml_module)

# qmltypes target
!cross_compile:if(build_pass|!debug_and_release) {
    qtPrepareTool(QMLPLUGINDUMP, qmlplugindump)

    # Use QtQml version defined in qmlplugindump source
    # TODO: retrieve the correct version from QtQml
    qmltypes.commands = $$QMLPLUGINDUMP -nonrelocatable QtQml 2.2 > $$PWD/plugins.qmltypes
    QMAKE_EXTRA_TARGETS += qmltypes
}
