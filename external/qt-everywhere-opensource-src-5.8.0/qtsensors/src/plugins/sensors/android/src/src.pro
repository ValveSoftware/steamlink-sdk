TARGET = qtsensors_android

QT = sensors core

# STATICPLUGIN needed because there's a Q_IMPORT_PLUGIN in main.cpp
# Yes, the plugin imports itself statically
DEFINES += QT_STATICPLUGIN

HEADERS = \
    androidjnisensors.h \
    androidaccelerometer.h \
    androidcompass.h \
    androidcommonsensor.h \
    androidgyroscope.h \
    androidmagnetometer.h \
    androidpressure.h \
    androidproximity.h \
    androidrotation.h \
    androidtemperature.h \
    androidlight.h

SOURCES = \
    main.cpp \
    androidjnisensors.cpp \
    androidaccelerometer.cpp \
    androidcompass.cpp \
    androidgyroscope.cpp \
    androidmagnetometer.cpp \
    androidpressure.cpp \
    androidproximity.cpp \
    androidrotation.cpp \
    androidtemperature.cpp \
    androidlight.cpp

OTHER_FILES = plugin.json

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = QCounterGesturePlugin
load(qt_plugin)
