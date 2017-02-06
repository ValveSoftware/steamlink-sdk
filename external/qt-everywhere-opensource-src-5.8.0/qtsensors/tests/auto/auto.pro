TEMPLATE = subdirs

SUBDIRS += qsensor
qtHaveModule(qml) {
    SUBDIRS += sensors2qmlapi
    SUBDIRS += qtsensors5
}
#SUBDIRS += legacy_sensors
SUBDIRS += qsensorgestures
SUBDIRS += qsensorgestureplugins
SUBDIRS += cmake

!mac:!win32:SUBDIRS += qsensorgestures_gestures

