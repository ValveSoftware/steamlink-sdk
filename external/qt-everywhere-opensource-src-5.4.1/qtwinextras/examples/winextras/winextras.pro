TEMPLATE = subdirs
win32 {
    SUBDIRS += \
        iconextractor
    qtHaveModule(widgets):qtHaveModule(multimedia): SUBDIRS += musicplayer
    qtHaveModule(quick):qtHaveModule(multimedia): SUBDIRS += quickplayer
}
