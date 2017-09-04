
# Base
STYLES_QML_FILES = \
    $$PWD/Base/ApplicationWindowStyle.qml \
    $$PWD/Base/ButtonStyle.qml \
    $$PWD/Base/BusyIndicatorStyle.qml \
    $$PWD/Base/CalendarStyle.qml \
    $$PWD/Base/CheckBoxStyle.qml \
    $$PWD/Base/ComboBoxStyle.qml \
    $$PWD/Base/FocusFrameStyle.qml \
    $$PWD/Base/GroupBoxStyle.qml \
    $$PWD/Base/MenuBarStyle.qml \
    $$PWD/Base/MenuStyle.qml \
    $$PWD/Base/ProgressBarStyle.qml \
    $$PWD/Base/RadioButtonStyle.qml \
    $$PWD/Base/ScrollViewStyle.qml\
    $$PWD/Base/SliderStyle.qml \
    $$PWD/Base/SpinBoxStyle.qml \
    $$PWD/Base/SwitchStyle.qml \
    $$PWD/Base/StatusBarStyle.qml \
    $$PWD/Base/BasicTableViewStyle.qml \
    $$PWD/Base/TableViewStyle.qml \
    $$PWD/Base/TreeViewStyle.qml \
    $$PWD/Base/TabViewStyle.qml \
    $$PWD/Base/TextAreaStyle.qml \
    $$PWD/Base/TextFieldStyle.qml \
    $$PWD/Base/ToolBarStyle.qml \
    $$PWD/Base/ToolButtonStyle.qml

# Extras
STYLES_QML_FILES += \
    $$PWD/Base/CircularGaugeStyle.qml \
    $$PWD/Base/CircularButtonStyle.qml \
    $$PWD/Base/CircularTickmarkLabelStyle.qml \
    $$PWD/Base/CommonStyleHelper.qml \
    $$PWD/Base/DelayButtonStyle.qml \
    $$PWD/Base/DialStyle.qml \
    $$PWD/Base/GaugeStyle.qml \
    $$PWD/Base/HandleStyle.qml \
    $$PWD/Base/HandleStyleHelper.qml \
    $$PWD/Base/PieMenuStyle.qml \
    $$PWD/Base/StatusIndicatorStyle.qml \
    $$PWD/Base/ToggleButtonStyle.qml \
    $$PWD/Base/TumblerStyle.qml

# Desktop
!no_desktop {
    STYLES_QML_FILES += \
        $$PWD/Desktop/qmldir \
        $$PWD/Desktop/ApplicationWindowStyle.qml \
        $$PWD/Desktop/RowItemSingleton.qml \
        $$PWD/Desktop/ButtonStyle.qml \
        $$PWD/Desktop/CalendarStyle.qml \
        $$PWD/Desktop/BusyIndicatorStyle.qml \
        $$PWD/Desktop/CheckBoxStyle.qml \
        $$PWD/Desktop/ComboBoxStyle.qml \
        $$PWD/Desktop/FocusFrameStyle.qml \
        $$PWD/Desktop/GroupBoxStyle.qml \
        $$PWD/Desktop/MenuBarStyle.qml \
        $$PWD/Desktop/MenuStyle.qml \
        $$PWD/Desktop/ProgressBarStyle.qml \
        $$PWD/Desktop/RadioButtonStyle.qml \
        $$PWD/Desktop/ScrollViewStyle.qml \
        $$PWD/Desktop/SliderStyle.qml \
        $$PWD/Desktop/SpinBoxStyle.qml \
        $$PWD/Desktop/SwitchStyle.qml \
        $$PWD/Desktop/StatusBarStyle.qml\
        $$PWD/Desktop/TabViewStyle.qml \
        $$PWD/Desktop/TableViewStyle.qml \
        $$PWD/Desktop/TreeViewStyle.qml \
        $$PWD/Desktop/TextAreaStyle.qml \
        $$PWD/Desktop/TextFieldStyle.qml \
        $$PWD/Desktop/ToolBarStyle.qml \
        $$PWD/Desktop/ToolButtonStyle.qml
}

# Images
STYLES_QML_FILES += \
    $$PWD/Base/images/button.png \
    $$PWD/Base/images/button_down.png \
    $$PWD/Base/images/tab.png \
    $$PWD/Base/images/header.png \
    $$PWD/Base/images/groupbox.png \
    $$PWD/Base/images/focusframe.png \
    $$PWD/Base/images/tab_selected.png \
    $$PWD/Base/images/slider-handle.png \
    $$PWD/Base/images/slider-groove.png \
    $$PWD/Base/images/scrollbar-handle-horizontal.png \
    $$PWD/Base/images/scrollbar-handle-vertical.png \
    $$PWD/Base/images/scrollbar-handle-transient.png \
    $$PWD/Base/images/progress-indeterminate.png \
    $$PWD/Base/images/editbox.png \
    $$PWD/Base/images/arrow-up.png \
    $$PWD/Base/images/arrow-up@2x.png \
    $$PWD/Base/images/arrow-down.png \
    $$PWD/Base/images/arrow-down@2x.png \
    $$PWD/Base/images/arrow-left.png \
    $$PWD/Base/images/arrow-left@2x.png \
    $$PWD/Base/images/arrow-right.png \
    $$PWD/Base/images/arrow-right@2x.png \
    $$PWD/Base/images/leftanglearrow.png \
    $$PWD/Base/images/rightanglearrow.png \
    $$PWD/Base/images/spinner_small.png \
    $$PWD/Base/images/spinner_medium.png \
    $$PWD/Base/images/spinner_large.png \
    $$PWD/Base/images/check.png \
    $$PWD/Base/images/check@2x.png \
    $$PWD/Base/images/knob.png \
    $$PWD/Base/images/needle.png

STYLES_QML_FILES += $$PWD/qmldir
ios:static: include(iOS/iOS.pri)
!qtquickcompiler|static: QML_FILES += $$STYLES_QML_FILES
