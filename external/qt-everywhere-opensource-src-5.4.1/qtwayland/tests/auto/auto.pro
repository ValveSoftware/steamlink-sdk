TEMPLATE=subdirs

#Only build compositor test when we are
#building QtCompositor
contains(CONFIG, wayland-compositor) {
    SUBDIRS += compositor
    SUBDIRS += client
    SUBDIRS += cmake
}
