requires(contains(QT_CONFIG, accessibility))

TARGET  = qtquickcontrolsplugin
TARGETPATH = QtQuick/Controls
IMPORT_VERSION = 1.5

QT += qml quick quick-private qml-private gui-private core-private

QMAKE_DOCS = $$PWD/doc/qtquickcontrols.qdocconf

CONTROLS_QML_FILES = \
    ApplicationWindow.qml \
    Button.qml \
    BusyIndicator.qml \
    Calendar.qml \
    CheckBox.qml \
    ComboBox.qml \
    GroupBox.qml \
    Label.qml \
    MenuBar.qml \
    Menu.qml \
    StackView.qml \
    ProgressBar.qml \
    RadioButton.qml \
    ScrollView.qml \
    Slider.qml \
    SpinBox.qml \
    SplitView.qml \
    StackViewDelegate.qml \
    StackViewTransition.qml \
    StatusBar.qml \
    Switch.qml \
    Tab.qml \
    TabView.qml \
    TableView.qml \
    TableViewColumn.qml \
    TreeView.qml \
    TextArea.qml \
    TextField.qml \
    ToolBar.qml \
    ToolButton.qml

qtquickcompiler {
    DEFINES += ALWAYS_LOAD_FROM_RESOURCES
} else {
    QML_FILES += $$CONTROLS_QML_FILES
    !static: CONFIG += qmlcache
}

SOURCES += $$PWD/plugin.cpp
HEADERS += $$PWD/plugin.h

OTHER_FILES += doc/src/*

include(plugin.pri)
include(Private/private.pri)
include(Styles/styles.pri)
include(Shaders/shaders.pri)

osx: LIBS_PRIVATE += -framework Carbon

!qmlcache {
    INCLUDED_RESOURCE_FILES = \
        $$CONTROLS_QML_FILES \
        $$PRIVATE_QML_FILES \
        $$STYLES_QML_FILES

} else {
    QML_FILES *= $$CONTROLS_QML_FILES \
                 $$PRIVATE_QML_FILES \
                 $$STYLES_QML_FILES
    OTHER_FILES += $$QML_FILES \
                 $$SHADER_FILES
}

INCLUDED_RESOURCE_FILES += $$SHADER_FILES

controls.files = $$INCLUDED_RESOURCE_FILES
controls.prefix = /QtQuick/Controls
RESOURCES += controls

CONFIG += no_cxx_module
load(qml_plugin)
