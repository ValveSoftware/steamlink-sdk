TEMPLATE = app
TARGET = uiforms

SOURCES += \
    main.cpp

RESOURCES += \
    uiforms.qrc

OTHER_FILES += \
    main.qml \
    MainForm.ui.qml \
    qml/Settings.qml \
    qml/History.qml \
    qml/Notes.qml \
    qml/SettingsForm.ui.qml \
    qml/HistoryForm.ui.qml \
    qml/NotesForm.ui.qml \
    qml/CustomerModel.qml

include(../shared/shared.pri)

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/controls/uiforms
INSTALLS += target
