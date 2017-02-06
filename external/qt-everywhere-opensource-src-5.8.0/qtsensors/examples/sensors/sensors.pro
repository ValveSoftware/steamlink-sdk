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

EXAMPLE_FILES += \
    stub.h
