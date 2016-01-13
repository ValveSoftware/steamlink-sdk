TEMPLATE = subdirs

blackberry {
    isEmpty(SENSORS_PLUGINS): SENSORS_PLUGINS = blackberry generic
}

android {
    isEmpty(SENSORS_PLUGINS): SENSORS_PLUGINS = android generic
}

sensorfw {
    isEmpty(SENSORS_PLUGINS): SENSORS_PLUGINS = sensorfw generic
}

ios {
    isEmpty(SENSORS_PLUGINS): SENSORS_PLUGINS = ios generic
}

winrt {
    isEmpty(SENSORS_PLUGINS): SENSORS_PLUGINS = winrt generic
}

qtHaveModule(simulator) {
    isEmpty(SENSORS_PLUGINS): SENSORS_PLUGINS = simulator generic
}

linux {
    isEmpty(SENSORS_PLUGINS): SENSORS_PLUGINS = linux generic
}

contains(SENSORS_PLUGINS, dummy):SUBDIRS += dummy
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, generic):SUBDIRS += generic
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, simulator):qtHaveModule(simulator):SUBDIRS += simulator
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, blackberry):blackberry:SUBDIRS += blackberry
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, linux):linux:SUBDIRS += linux
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, android):android:SUBDIRS += android
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, sensorfw):sensorfw:SUBDIRS += sensorfw
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, ios):ios:SUBDIRS += ios
isEmpty(SENSORS_PLUGINS)|contains(SENSORS_PLUGINS, winrt):winrt:SUBDIRS += winrt
