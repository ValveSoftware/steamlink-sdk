TEMPLATE = subdirs

contains(QT_CONFIG, private_tests) {
    SUBDIRS += \
        qcuboidgeometry \
        qtorusgeometry \
        qforwardrenderer \
        qfirstpersoncameracontroller \
        qorbitcameracontroller
}
