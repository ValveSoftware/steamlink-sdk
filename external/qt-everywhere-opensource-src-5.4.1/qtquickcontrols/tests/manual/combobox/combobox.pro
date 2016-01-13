QT += qml quick
TARGET = combobox
!no_desktop: QT += widgets

SOURCES += $$PWD/main.cpp

OTHER_FILES += \
    qml/main.qml
