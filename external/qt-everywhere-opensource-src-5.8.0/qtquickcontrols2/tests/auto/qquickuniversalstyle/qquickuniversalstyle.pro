TEMPLATE = app
TARGET = tst_qquickuniversalstyle
CONFIG += qmltestcase

SOURCES += \
    $$PWD/tst_qquickuniversalstyle.cpp

RESOURCES += \
    $$PWD/qtquickcontrols2.conf

OTHER_FILES += \
    $$PWD/data/*.qml

TESTDATA += \
    $$PWD/data/tst_*
