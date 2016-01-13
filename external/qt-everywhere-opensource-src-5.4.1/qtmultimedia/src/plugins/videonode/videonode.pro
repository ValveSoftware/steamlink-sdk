TEMPLATE = subdirs

config_gpu_vivante {
    SUBDIRS += imx6
}

contains(QT_CONFIG, egl):contains(QT_CONFIG, opengles2):!android: SUBDIRS += egl
