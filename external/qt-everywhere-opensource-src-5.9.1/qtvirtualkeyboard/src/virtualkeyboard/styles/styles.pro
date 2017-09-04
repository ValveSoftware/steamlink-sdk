TARGET = qtvirtualkeyboardstylesplugin
TARGETPATH = QtQuick/VirtualKeyboard/Styles
QT += qml quick svg

CONFIG += no_cxx_module

SOURCES += \
    svgimageprovider.cpp \
    styles_plugin.cpp

HEADERS += \
    svgimageprovider.h \
    styles_plugin.h

RESOURCES += \
    styles.qrc

win32 {
    QMAKE_TARGET_PRODUCT = "Qt Virtual Keyboard (Qt $$QT_VERSION)"
    QMAKE_TARGET_DESCRIPTION = "Virtual Keyboard for Qt."
}

load(qml_plugin)
