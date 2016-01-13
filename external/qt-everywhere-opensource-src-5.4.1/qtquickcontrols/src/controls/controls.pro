TARGET  = qtquickcontrolsplugin
TARGETPATH = QtQuick/Controls
IMPORT_VERSION = 1.2

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
    TextArea.qml \
    TextField.qml \
    ToolBar.qml \
    ToolButton.qml

QML_FILES += $$CONTROLS_QML_FILES

SOURCES += $$PWD/plugin.cpp
HEADERS += $$PWD/plugin.h

include(plugin.pri)
include(Private/private.pri)
include(Styles/styles.pri)

osx: LIBS_PRIVATE += -framework Carbon

# Create the resource file
GENERATED_RESOURCE_FILE = $$OUT_PWD/controls.qrc

INCLUDED_RESOURCE_FILES = \
    $$CONTROLS_QML_FILES \
    $$PRIVATE_QML_FILES \
    $$STYLES_QML_FILES

RESOURCE_CONTENT = \
    "<RCC>" \
    "<qresource prefix=\"/QtQuick/Controls\">"

for(resourcefile, INCLUDED_RESOURCE_FILES) {
    resourcefileabsolutepath = $$absolute_path($$resourcefile)
    relativepath_in = $$relative_path($$resourcefileabsolutepath, $$_PRO_FILE_PWD_)
    relativepath_out = $$relative_path($$resourcefileabsolutepath, $$OUT_PWD)
    RESOURCE_CONTENT += "<file alias=\"$$relativepath_in\">$$relativepath_out</file>"
}

RESOURCE_CONTENT += \
    "</qresource>" \
    "</RCC>"

write_file($$GENERATED_RESOURCE_FILE, RESOURCE_CONTENT)|error("Aborting.")

RESOURCES += $$GENERATED_RESOURCE_FILE

CONFIG += no_cxx_module
load(qml_plugin)
