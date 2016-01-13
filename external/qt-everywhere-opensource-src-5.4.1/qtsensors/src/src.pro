TEMPLATE = subdirs

SUBDIRS += sensors
sensors.subdir = sensors
sensors.target = sub-sensors

qtHaveModule(quick) {
    SUBDIRS += imports
    imports.subdir = imports
    imports.target = sub-imports
    imports.depends = sensors
}

SUBDIRS += plugins
plugins.subdir = plugins
plugins.target = sub-plugins
plugins.depends = sensors

