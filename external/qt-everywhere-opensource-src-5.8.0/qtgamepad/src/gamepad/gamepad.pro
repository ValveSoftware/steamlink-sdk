TARGET     = QtGamepad
QT         = core-private core gui

MODULE_PLUGIN_TYPES = gamepads

load(qt_module)

DEFINES += QT_BUILD_GAMEPAD_LIB

QMAKE_DOCS = $$PWD/doc/qtgamepad.qdocconf

HEADERS += \
    qtgamepadglobal.h \
    qgamepadmanager.h \
    qgamepad.h \
    qgamepadbackend_p.h \
    qgamepadbackendfactory_p.h \
    qgamepadkeynavigation.h \
    qgamepadbackendplugin_p.h
SOURCES += \
    qgamepadmanager.cpp \
    qgamepad.cpp \
    qgamepadbackend.cpp \
    qgamepadbackendfactory.cpp \
    qgamepadkeynavigation.cpp \
    qgamepadbackendplugin.cpp


ANDROID_BUNDLED_JAR_DEPENDENCIES = \
    jar/QtAndroidGamepad-bundled.jar

ANDROID_JAR_DEPENDENCIES = \
    jar/QtAndroidGamepad.jar

ANDROID_LIB_DEPENDENCIES = \
    plugins/gamepads/libandroidgamepad.so

ANDROID_FEATURES = \
    android.hardware.gamepad
