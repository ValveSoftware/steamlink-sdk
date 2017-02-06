TARGET  = qtquickcontrolswinrtstyleplugin
TARGETPATH = QtQuick/Controls/Styles/WinRT

!winphone: {
OTHER_FILES += \
    $$PWD/PC/ApplicationWindowStyle.qml \
    $$PWD/PC/BusyIndicatorStyle.qml \
    $$PWD/PC/ButtonStyle.qml \
    $$PWD/PC/CalendarStyle.qml \
    $$PWD/PC/CheckBoxStyle.qml \
    $$PWD/PC/ComboBoxStyle.qml \
    $$PWD/PC/FocusFrameStyle.qml \
    $$PWD/PC/GroupBoxStyle.qml \
    $$PWD/PC/MenuBarStyle.qml \
    $$PWD/PC/MenuStyle.qml \
    $$PWD/PC/ProgressBarStyle.qml \
    $$PWD/PC/RadioButtonStyle.qml \
    $$PWD/PC/ScrollViewStyle.qml\
    $$PWD/PC/SliderStyle.qml \
    $$PWD/PC/SpinBoxStyle.qml \
    $$PWD/PC/StatusBarStyle.qml \
    $$PWD/PC/SwitchStyle.qml \
    $$PWD/PC/TableViewStyle.qml \
    $$PWD/PC/TabViewStyle.qml \
    $$PWD/PC/TextAreaStyle.qml \
    $$PWD/PC/TextFieldStyle.qml \
    $$PWD/PC/ToolBarStyle.qml \
    $$PWD/PC/ToolButtonStyle.qml

RESOURCES += \
    $$PWD/PC/WinRT.qrc
} else {
OTHER_FILES += \
    $$PWD/Phone/ApplicationWindowStyle.qml \
    $$PWD/Phone/BusyIndicatorStyle.qml \
    $$PWD/Phone/ButtonStyle.qml \
    $$PWD/Phone/CalendarStyle.qml \
    $$PWD/Phone/CheckBoxStyle.qml \
    $$PWD/Phone/ComboBoxStyle.qml \
    $$PWD/Phone/FocusFrameStyle.qml \
    $$PWD/Phone/GroupBoxStyle.qml \
    $$PWD/Phone/MenuBarStyle.qml \
    $$PWD/Phone/MenuStyle.qml \
    $$PWD/Phone/ProgressBarStyle.qml \
    $$PWD/Phone/RadioButtonStyle.qml \
    $$PWD/Phone/ScrollViewStyle.qml\
    $$PWD/Phone/SliderStyle.qml \
    $$PWD/Phone/SpinBoxStyle.qml \
    $$PWD/Phone/StatusBarStyle.qml \
    $$PWD/Phone/SwitchStyle.qml \
    $$PWD/Phone/TableViewStyle.qml \
    $$PWD/Phone/TabViewStyle.qml \
    $$PWD/Phone/TextAreaStyle.qml \
    $$PWD/Phone/TextFieldStyle.qml \
    $$PWD/Phone/ToolBarStyle.qml \
    $$PWD/Phone/ToolButtonStyle.qml

RESOURCES += \
    $$PWD/Phone/WinRT.qrc
}

SOURCES += \
    $$PWD/plugin.cpp

CONFIG += no_cxx_module
load(qml_plugin)
