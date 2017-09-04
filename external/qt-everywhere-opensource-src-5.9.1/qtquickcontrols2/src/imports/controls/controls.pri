HEADERS += \
    $$PWD/qquickdialring_p.h \
    $$PWD/qquickdefaultbusyindicator_p.h \
    $$PWD/qquickdefaultprogressbar_p.h \
    $$PWD/qquickdefaultstyle_p.h

SOURCES += \
    $$PWD/qquickdialring.cpp \
    $$PWD/qquickdefaultbusyindicator.cpp \
    $$PWD/qquickdefaultprogressbar.cpp \
    $$PWD/qquickdefaultstyle.cpp

QML_CONTROLS = \
    $$PWD/AbstractButton.qml \
    $$PWD/ApplicationWindow.qml \
    $$PWD/BusyIndicator.qml \
    $$PWD/Button.qml \
    $$PWD/ButtonGroup.qml \
    $$PWD/CheckBox.qml \
    $$PWD/CheckDelegate.qml \
    $$PWD/CheckIndicator.qml \
    $$PWD/ComboBox.qml \
    $$PWD/Container.qml \
    $$PWD/Control.qml \
    $$PWD/DelayButton.qml \
    $$PWD/Dial.qml \
    $$PWD/Dialog.qml \
    $$PWD/DialogButtonBox.qml \
    $$PWD/Drawer.qml \
    $$PWD/Frame.qml \
    $$PWD/GroupBox.qml \
    $$PWD/ItemDelegate.qml \
    $$PWD/Label.qml \
    $$PWD/Menu.qml \
    $$PWD/MenuItem.qml \
    $$PWD/MenuSeparator.qml \
    $$PWD/Page.qml \
    $$PWD/PageIndicator.qml \
    $$PWD/Pane.qml \
    $$PWD/Popup.qml \
    $$PWD/ProgressBar.qml \
    $$PWD/RadioButton.qml \
    $$PWD/RadioDelegate.qml \
    $$PWD/RadioIndicator.qml \
    $$PWD/RangeSlider.qml \
    $$PWD/RoundButton.qml \
    $$PWD/ScrollBar.qml \
    $$PWD/ScrollIndicator.qml \
    $$PWD/ScrollView.qml \
    $$PWD/Slider.qml \
    $$PWD/SpinBox.qml \
    $$PWD/StackView.qml \
    $$PWD/SwipeDelegate.qml \
    $$PWD/Switch.qml \
    $$PWD/SwitchIndicator.qml \
    $$PWD/SwitchDelegate.qml \
    $$PWD/SwipeView.qml \
    $$PWD/TabBar.qml \
    $$PWD/TabButton.qml \
    $$PWD/TextArea.qml \
    $$PWD/TextField.qml \
    $$PWD/ToolBar.qml \
    $$PWD/ToolButton.qml \
    $$PWD/ToolSeparator.qml \
    $$PWD/ToolTip.qml \
    $$PWD/Tumbler.qml

!qtquickcompiler: QML_FILES += $$QML_CONTROLS
