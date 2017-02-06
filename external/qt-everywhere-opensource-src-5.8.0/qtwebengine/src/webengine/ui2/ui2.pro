TARGETPATH = QtWebEngine/Controls2Delegates

QML_FILES += \
    # Authentication Dialog
    AuthenticationDialog.qml \
    # JS Dialogs
    AlertDialog.qml \
    ConfirmDialog.qml \
    PromptDialog.qml \
    # Menus. Based on Qt Quick Controls
    Menu.qml \
    MenuItem.qml \
    MenuSeparator.qml \
    ToolTip.qml \
    information.png \
    question.png

load(qml_module)
