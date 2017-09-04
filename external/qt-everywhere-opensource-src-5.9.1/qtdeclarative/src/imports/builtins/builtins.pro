TEMPLATE = aux

QMLTYPEFILE = builtins.qmltypes

# install rule
builtins.files = $$QMLTYPEFILE
builtins.path = $$[QT_INSTALL_QML]
INSTALLS += builtins

# copy to build directory
!prefix_build: COPIES += builtins

# qmltypes target
!cross_compile:if(build_pass|!debug_and_release) {
    qtPrepareTool(QMLPLUGINDUMP, qmlplugindump)

    qmltypes.commands = $$QMLPLUGINDUMP -builtins > $$PWD/$$QMLTYPEFILE
    QMAKE_EXTRA_TARGETS += qmltypes
}
