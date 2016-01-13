TEMPLATE = subdirs

SUBDIRS += qsensor
SUBDIRS += sensors2qmlapi
#SUBDIRS += legacy_sensors
SUBDIRS += qtsensors5
SUBDIRS += qsensorgestures
SUBDIRS += qsensorgestureplugins
SUBDIRS += cmake

!mac:!win32:SUBDIRS += qsensorgestures_gestures

