TARGET = QtSensors
CONFIG += strict_flags
QT = core-private

CONFIG(debug,debug|release):DEFINES += ENABLE_RUNTIME_SENSORLOG
!isEmpty(SENSORS_CONFIG_PATH):DEFINES += "QTSENSORS_CONFIG_PATH=\\\"$$SENSORS_CONFIG_PATH\\\""

qtHaveModule(simulator) {
    DEFINES += SIMULATOR_BUILD
    QT_FOR_PRIVATE += simulator
}

QMAKE_DOCS = $$PWD/doc/qtsensors.qdocconf

ANDROID_BUNDLED_JAR_DEPENDENCIES = \
    jar/QtSensors-bundled.jar:org.qtproject.qt5.android.sensors.QtSensors
ANDROID_JAR_DEPENDENCIES = \
    jar/QtSensors.jar:org.qtproject.qt5.android.sensors.QtSensors
ANDROID_LIB_DEPENDENCIES = \
    plugins/sensors/libqtsensors_android.so

PUBLIC_HEADERS += \
           qsensorbackend.h\
           qsensormanager.h\
           qsensorplugin.h\
           qsensorsglobal.h

PRIVATE_HEADERS += \
           sensorlog_p.h\

SOURCES += qsensorbackend.cpp\
           qsensormanager.cpp\
           qsensorplugin.cpp

SOURCES += \
    gestures/qsensorgesture.cpp \
    gestures/qsensorgesturerecognizer.cpp \
    gestures/qsensorgesturemanager.cpp \
    gestures/qsensorgesturemanagerprivate.cpp \
    gestures/qsensorgestureplugininterface.cpp

GESTURE_HEADERS += \
    gestures/qsensorgesture.h\
    gestures/qsensorgesture_p.h\
    gestures/qsensorgesturerecognizer.h \
    gestures/qsensorgesturemanager.h \
    gestures/qsensorgesturemanagerprivate_p.h \
    gestures/qsensorgestureplugininterface.h

qtHaveModule(simulator) {
    SOURCES += gestures/simulatorgesturescommon.cpp
    GESTURE_HEADERS += gestures/simulatorgesturescommon_p.h
}

# 3 files per sensor (including QSensor)
SENSORS=\
    qsensor\
    qaccelerometer\
    qaltimeter\
    qambientlightsensor\
    qambienttemperaturesensor\
    qcompass\
    qdistancesensor\
    qholstersensor\
    qlightsensor\
    qmagnetometer\
    qorientationsensor\
    qproximitysensor\
    qirproximitysensor\
    qrotationsensor\
    qtapsensor\
    qtiltsensor\
    qgyroscope\
    qpressuresensor

for(s,SENSORS) {
    # Client API
    PUBLIC_HEADERS += $${s}.h
    SOURCES        += $${s}.cpp
    # Private header
    PRIVATE_HEADERS += $${s}_p.h
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS $$GESTURE_HEADERS

MODULE_PLUGIN_TYPES = \
    sensors \
    sensorgestures
load(qt_module)
