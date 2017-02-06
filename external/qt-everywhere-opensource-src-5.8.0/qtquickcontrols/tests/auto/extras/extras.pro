TEMPLATE = app
TARGET = tst_extras

CONFIG += qmltestcase console

SOURCES += $$PWD/tst_extras.cpp

TESTDATA = $$PWD/data/*

OTHER_FILES += \
    $$PWD/data/tst_circulargauge.qml \
    $$PWD/data/tst_circulartickmarklabel.qml \
    $$PWD/data/tst_common.qml \
    $$PWD/data/tst_dial.qml \
    $$PWD/data/tst_piemenu.qml \
    $$PWD/data/tst_delaybutton.qml \
    $$PWD/data/tst_statusindicator.qml \
    $$PWD/data/tst_thermometergauge.qml \
    $$PWD/data/tst_togglebutton.qml \
    $$PWD/data/tst_tumbler.qml \
    $$PWD/data/PieMenu3Items.qml \
    $$PWD/data/PieMenu3ItemsLongPress.qml \
    $$PWD/data/PieMenu3ItemsKeepOpen.qml \
    $$PWD/data/PieMenuVisibleOnCompleted.qml \
    $$PWD/data/PieMenuVisibleButNoParent.qml \
    $$PWD/data/tst_gauge.qml \
    $$PWD/data/tst_picture.qml \
    $$PWD/data/TestUtils.js \
    $$PWD/data/TumblerDatePicker.qml \
    $$PWD/data/PieMenuRotatedBoundingItem.qml \
    $$PWD/data/PieMenuBoundingItem.qml
