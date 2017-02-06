TEMPLATE = subdirs
CONFIG += ordered
include($$OUT_PWD/quick/qtquick-config.pri)
QT_FOR_CONFIG += quick-private
SUBDIRS += \
    qml

qtHaveModule(gui) {
    SUBDIRS += \
        quick \
        qmltest

    qtConfig(quick-sprite):qtConfig(opengl(es1|es2)?): \
        SUBDIRS += particles
    qtHaveModule(widgets): SUBDIRS += quickwidgets
}

SUBDIRS += \
    plugins \
    imports \
    qmldevtools

!contains(QT_CONFIG, no-qml-debug): SUBDIRS += qmldebug

qmldevtools.CONFIG = host_build
