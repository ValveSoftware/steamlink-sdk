TEMPLATE = subdirs

include($$OUT_PWD/../../serialbus/qtserialbus-config.pri)
QT_FOR_CONFIG += serialbus-private
qtConfig(socketcan) {
    SUBDIRS += socketcan
}

qtConfig(library) {
    SUBDIRS += peakcan tinycan
    win32:SUBDIRS += systeccan vectorcan
}
