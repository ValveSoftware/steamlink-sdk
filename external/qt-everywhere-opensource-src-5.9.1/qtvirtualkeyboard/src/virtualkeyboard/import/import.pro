TARGETPATH = QtQuick/VirtualKeyboard
QML_FILES += plugins.qmltypes

load(qml_module)

# qmltypes target
!cross_compile:if(build_pass|!debug_and_release) {
    qtPrepareTool(QMLPLUGINDUMP, qmlplugindump)

    qmltypes.commands = QT_IM_MODULE=qtvirtualkeyboard $$QMLPLUGINDUMP -defaultplatform -nonrelocatable QtQuick.VirtualKeyboard 2.1 > $$PWD/plugins.qmltypes
    QMAKE_EXTRA_TARGETS += qmltypes
}
