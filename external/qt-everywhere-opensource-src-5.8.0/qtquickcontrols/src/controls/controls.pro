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

!qtquickcompiler: QML_FILES += $$CONTROLS_QML_FILES
qtquickcompiler: DEFINES += ALWAYS_LOAD_FROM_RESOURCES

SOURCES += $$PWD/plugin.cpp
HEADERS += $$PWD/plugin.h

OTHER_FILES += doc/src/*

include(plugin.pri)
include(Private/private.pri)
include(Styles/styles.pri)
include(Shaders/shaders.pri)

osx: LIBS_PRIVATE += -framework Carbon

!static {
    # Create the resource file
    GENERATED_RESOURCE_FILE = $$OUT_PWD/controls.qrc

    INCLUDED_RESOURCE_FILES = \
        $$CONTROLS_QML_FILES \
        $$PRIVATE_QML_FILES \
        $$STYLES_QML_FILES \
        $$SHADER_FILES

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
} else {
    QML_FILES *= $$CONTROLS_QML_FILES \
                 $$PRIVATE_QML_FILES \
                 $$STYLES_QML_FILES \
                 $$SHADER_FILES
    OTHER_FILES += $$QML_FILES
}

CONFIG += no_cxx_module
load(qml_plugin)
