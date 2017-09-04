TARGET = qtquickextrasflatplugin
TARGETPATH = QtQuick/Controls/Styles/Flat

QT += qml quick

HEADERS += \
    flatstyleplugin.h \
    qquicktexthandle.h
SOURCES += \
    flatstyleplugin.cpp \
    qquicktexthandle.cpp

FLAT_STYLE += \
    $$PWD/FlatStyle.qml \
    $$PWD/ApplicationWindowStyle.qml \
    $$PWD/BusyIndicatorStyle.qml \
    $$PWD/ButtonStyle.qml \
    $$PWD/CalendarStyle.qml \
    $$PWD/CheckBoxStyle.qml \
    $$PWD/CheckBoxDrawer.qml \
    $$PWD/CircularButtonStyle.qml \
    $$PWD/CircularGaugeStyle.qml \
    $$PWD/CircularTickmarkLabelStyle.qml \
    $$PWD/ComboBoxStyle.qml \
    $$PWD/CursorHandleStyle.qml \
    $$PWD/DelayButtonStyle.qml \
    $$PWD/DialStyle.qml \
    $$PWD/FocusFrameStyle.qml \
    $$PWD/GaugeStyle.qml \
    $$PWD/GroupBoxStyle.qml \
    $$PWD/LeftArrowIcon.qml \
    $$PWD/MenuBarStyle.qml \
    $$PWD/MenuStyle.qml \
    $$PWD/PieMenuStyle.qml \
    $$PWD/ProgressBarStyle.qml \
    $$PWD/RadioButtonStyle.qml \
    $$PWD/ScrollViewStyle.qml \
    $$PWD/SelectionHandleStyle.qml \
    $$PWD/SliderStyle.qml \
    $$PWD/SpinBoxStyle.qml \
    $$PWD/StatusBarStyle.qml \
    $$PWD/StatusIndicatorStyle.qml \
    $$PWD/SwitchStyle.qml \
    $$PWD/TabViewStyle.qml \
    $$PWD/TableViewStyle.qml \
    $$PWD/TextAreaStyle.qml \
    $$PWD/TextFieldStyle.qml \
    $$PWD/ToggleButtonStyle.qml \
    $$PWD/ToolBarStyle.qml \
    $$PWD/ToolButtonStyle.qml \
    $$PWD/ToolButtonBackground.qml \
    $$PWD/ToolButtonIndicator.qml \
    $$PWD/TumblerStyle.qml

FLAT_STYLE += \
    $$PWD/images/BusyIndicator_Normal-Large.png \
    $$PWD/images/BusyIndicator_Normal-Medium.png \
    $$PWD/images/BusyIndicator_Normal-Small.png \
    $$PWD/fonts/OpenSans-Light.ttf \
    $$PWD/fonts/OpenSans-Regular.ttf \
    $$PWD/fonts/OpenSans-Semibold.ttf \
    $$PWD/fonts/LICENSE.txt \

!static:RESOURCES += flatstyle.qrc
else: QML_FILES += $$FLAT_STYLE

CONFIG += no_cxx_module
load(qml_plugin)
