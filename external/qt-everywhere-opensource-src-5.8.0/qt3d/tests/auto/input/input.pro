TEMPLATE = subdirs

qtConfig(private_tests) {
    SUBDIRS += \
        qaxis \
        qaction \
        qactioninput \
        qabstractaxisinput \
        qanalogaxisinput \
        qbuttonaxisinput \
        qkeyboardhandler \
        qlogicaldevice \
        axis \
        action \
        abstractaxisinput \
        actioninput \
        analogaxisinput \
        buttonaxisinput \
        keyboardhandler \
        qaxisaccumulator \
        inputsequence \
        inputchord \
        qabstractphysicaldevicebackendnode \
        logicaldevice \
        qabstractphysicaldeviceproxy \
        physicaldeviceproxy \
        loadproxydevicejob \
        qmousedevice \
        mousedevice \
        utils \
        axisaccumulator \
        axisaccumulatorjob
}
