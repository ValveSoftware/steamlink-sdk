
QML_IMPORT_PATH += $$PWD/imports

RESOURCES += \
    $$PWD/common.qrc

commonresources.files += $$PWD/resources/*

qmlcomponents.files += \
    $$PWD/imports/QtLocation/examples/components/TextWithLabel.qml \
    $$PWD/imports/QtLocation/examples/components/Button.qml \
    $$PWD/imports/QtLocation/examples/components/Checkbox.qml \
    $$PWD/imports/QtLocation/examples/components/Fader.qml \
    $$PWD/imports/QtLocation/examples/components/Optionbutton.qml \
    $$PWD/imports/QtLocation/examples/components/Slider.qml \
    $$PWD/imports/QtLocation/examples/components/TitleBar.qml \
    $$PWD/imports/QtLocation/examples/components/ButtonRow.qml \
    $$PWD/imports/QtLocation/examples/components/Menu.qml \
    $$PWD/imports/QtLocation/examples/components/IconButton.qml \
    $$PWD/imports/QtLocation/examples/components/BusyIndicator.qml
OTHER_FILES += $$qmlcomponents.files

qmlcomponentsstyle.files += \
    $$PWD/imports/QtLocation/examples/components/style/Style.qml \
    $$PWD/imports/QtLocation/examples/components/style/ButtonStyle.qml \
    $$PWD/imports/QtLocation/examples/components/style/HMenuItemStyle.qml \
    $$PWD/imports/QtLocation/examples/components/style/VMenuItemStyle.qml
OTHER_FILES += $$qmlcomponentsstyle.files

qmldialogs.files += \
    $$PWD/imports/QtLocation/examples/dialogs/Dialog.qml \
    $$PWD/imports/QtLocation/examples/dialogs/InputDialog.qml \
    $$PWD/imports/QtLocation/examples/dialogs/ErrorDialog.qml
OTHER_FILES += $$qmldialogs.files

qmldir.files += $$PWD/imports/QtLocation/examples/qmldir
OTHER_FILES += $$qmldir.files
