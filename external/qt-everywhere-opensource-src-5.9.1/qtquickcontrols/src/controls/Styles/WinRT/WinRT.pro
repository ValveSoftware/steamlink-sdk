TARGET  = qtquickcontrolswinrtstyleplugin
TARGETPATH = QtQuick/Controls/Styles/WinRT

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

SOURCES += \
    $$PWD/plugin.cpp

CONFIG += no_cxx_module
load(qml_plugin)
