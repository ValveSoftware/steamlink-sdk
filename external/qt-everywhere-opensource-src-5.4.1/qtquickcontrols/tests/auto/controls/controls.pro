TEMPLATE = app
TARGET = tst_controls

IMPORTPATH = $$OUT_PWD/../testplugin

!no_desktop: QT += widgets

CONFIG += qmltestcase console

INCLUDEPATH += $$PWD/../../shared
SOURCES += $$PWD/tst_controls.cpp

TESTDATA = $$PWD/data/*

OTHER_FILES += \
    $$PWD/data/tst_button.qml \
    $$PWD/data/tst_busyindicator.qml \
    $$PWD/data/tst_calendar.qml \
    $$PWD/data/tst_rangeddate.qml \
    $$PWD/data/tst_shortcuts.qml \
    $$PWD/data/tst_spinbox.qml \
    $$PWD/data/tst_tableview.qml \
    $$PWD/data/tst_rangemodel.qml \
    $$PWD/data/tst_scrollview.qml \
    $$PWD/data/tst_menu.qml \
    $$PWD/data/tst_textfield.qml \
    $$PWD/data/tst_textarea.qml \
    $$PWD/data/tst_combobox.qml \
    $$PWD/data/tst_progressbar.qml \
    $$PWD/data/tst_radiobutton.qml \
    $$PWD/data/tst_label.qml \
    $$PWD/data/tst_page.qml \
    $$PWD/data/tst_menubar.qml \
    $$PWD/data/tst_rowlayout.qml \
    $$PWD/data/tst_gridlayout.qml \
    $$PWD/data/tst_slider.qml \
    $$PWD/data/tst_stack.qml \
    $$PWD/data/tst_stackview.qml \
    $$PWD/data/tst_statusbar.qml \
    $$PWD/data/tst_switch.qml \
    $$PWD/data/tst_tab.qml \
    $$PWD/data/tst_tabview.qml \
    $$PWD/data/tst_tableviewcolumn.qml \
    $$PWD/data/tst_toolbar.qml \
    $$PWD/data/tst_toolbutton.qml \
    $$PWD/data/tst_checkbox.qml \
    $$PWD/data/tst_groupbox.qml \
    $$PWD/data/tst_splitview.qml \
    $$PWD/data/tst_styles.qml \
    $$PWD/data/tst_layout.qml \
    $$PWD/data/tst_keys.qml
