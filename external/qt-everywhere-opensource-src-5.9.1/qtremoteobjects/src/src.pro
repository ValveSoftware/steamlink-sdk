TEMPLATE = subdirs
QT_FOR_CONFIG += network
CONFIG += ordered

SUBDIRS += \
    repparser

qtConfig(localserver): {
    SUBDIRS += \
        remoteobjects
}

qtHaveModule(quick) {
    SUBDIRS += imports
}
