TEMPLATE = subdirs

SUBDIRS += grue

qtHaveModule(quick) {
    SUBDIRS += \
        maze \
        qmlsensorgestures \
        qmlqtsensors \
        sensor_explorer \
        shakeit

     qtHaveModule(svg): SUBDIRS += \
        accelbubble
}

qtHaveModule(widgets): SUBDIRS += \
    sensorgestures

OTHER_FILES = stub.h
