TARGET  = qtquickcontrolsandroidstyleplugin
TARGETPATH = QtQuick/Controls/Styles/Android
IMPORT_VERSION = 1.0

QT += qml quick quick-private gui-private core-private

QML_FILES += \
    $$PWD/AndroidStyle.qml \
    $$PWD/ApplicationWindowStyle.qml \
    $$PWD/ButtonStyle.qml \
    $$PWD/BusyIndicatorStyle.qml \
    $$PWD/CalendarStyle.qml \
    $$PWD/CheckBoxStyle.qml \
    $$PWD/ComboBoxStyle.qml \
    $$PWD/CursorHandleStyle.qml \
    $$PWD/FocusFrameStyle.qml \
    $$PWD/GroupBoxStyle.qml \
    $$PWD/LabelStyle.qml \
    $$PWD/MenuBarStyle.qml \
    $$PWD/MenuStyle.qml \
    $$PWD/ProgressBarStyle.qml \
    $$PWD/RadioButtonStyle.qml \
    $$PWD/ScrollViewStyle.qml\
    $$PWD/SliderStyle.qml \
    $$PWD/SpinBoxStyle.qml \
    $$PWD/SwitchStyle.qml \
    $$PWD/StatusBarStyle.qml \
    $$PWD/TableViewStyle.qml \
    $$PWD/TabViewStyle.qml \
    $$PWD/TextAreaStyle.qml \
    $$PWD/TextFieldStyle.qml \
    $$PWD/ToolBarStyle.qml \
    $$PWD/ToolButtonStyle.qml \
    $$PWD/drawables/Drawable.qml \
    $$PWD/drawables/DrawableLoader.qml \
    $$PWD/drawables/AnimationDrawable.qml \
    $$PWD/drawables/ClipDrawable.qml \
    $$PWD/drawables/ColorDrawable.qml \
    $$PWD/drawables/GradientDrawable.qml \
    $$PWD/drawables/ImageDrawable.qml \
    $$PWD/drawables/LayerDrawable.qml \
    $$PWD/drawables/NinePatchDrawable.qml \
    $$PWD/drawables/RotateDrawable.qml \
    $$PWD/drawables/StateDrawable.qml

HEADERS += \
    $$PWD/qquickandroid9patch_p.h \
    $$PWD/qquickandroidstyle_p.h

SOURCES += \
    $$PWD/plugin.cpp \
    $$PWD/qquickandroid9patch.cpp \
    $$PWD/qquickandroidstyle.cpp

CONFIG += no_cxx_module
load(qml_plugin)
