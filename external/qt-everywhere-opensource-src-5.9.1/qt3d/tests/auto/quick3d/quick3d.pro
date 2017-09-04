TEMPLATE = subdirs

qtConfig(private_tests) {
    SUBDIRS += \
        quick3dnodeinstantiator \
        dynamicnodecreation \
        3drender \
        3dinput \
        3dcore \
        quick3dbuffer \
        quick3dnode
}
