QT += gamepad

CONFIG += c++11

SOURCES = main.cpp \
    gamepadmonitor.cpp

HEADERS += \
    gamepadmonitor.h

DISTFILES += \
    android/AndroidManifest.xml

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

target.path = $$[QT_INSTALL_EXAMPLES]/gamepad/simple
INSTALLS += target
