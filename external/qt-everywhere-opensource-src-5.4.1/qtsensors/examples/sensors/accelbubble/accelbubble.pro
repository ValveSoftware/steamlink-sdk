TEMPLATE = app
TARGET = accelbubble
QT += quick sensors svg xml
SOURCES = main.cpp

RESOURCES += \
    accelbubble.qrc

OTHER_FILES = \
    $$files(*.qml) \
    content \
    images \
    android/AndroidManifest.xml

target.path = $$[QT_INSTALL_EXAMPLES]/sensors/accelbubble
INSTALLS += target

ios {
QMAKE_INFO_PLIST = Info.plist

# manual plugin loading needed with older Qt
# QTPLUGIN += qsvg qtsensors_ios qtsensors_generic
}

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
