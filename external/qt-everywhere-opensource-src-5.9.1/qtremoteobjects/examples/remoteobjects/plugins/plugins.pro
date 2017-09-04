QT += qml remoteobjects

TEMPLATE = lib
CONFIG += plugin

REPC_REPLICA += $$PWD/../timemodel.rep

DESTDIR = imports/TimeExample
TARGET  = qmlqtimeexampleplugin

SOURCES += plugin.cpp

pluginfiles.files += \
    imports/TimeExample/qmldir \
    imports/TimeExample/center.png \
    imports/TimeExample/clock.png \
    imports/TimeExample/Clock.qml \
    imports/TimeExample/hour.png \
    imports/TimeExample/minute.png

qml.files = plugins.qml
qml.path += $$[QT_INSTALL_EXAMPLES]/remoteobjects/plugins
target.path += $$[QT_INSTALL_EXAMPLES]/remoteobjects/plugins/imports/TimeExample
pluginfiles.path += $$[QT_INSTALL_EXAMPLES]/remoteobjects/plugins/imports/TimeExample

INSTALLS += target qml pluginfiles

CONFIG += install_ok # Do not cargo-cult this!

contains(QT_CONFIG, c++11): CONFIG += c++11
