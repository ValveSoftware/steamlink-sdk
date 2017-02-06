TEMPLATE = subdirs

include($$OUT_PWD/../../serialbus/qtserialbus-config.pri)
QT_FOR_CONFIG += serialbus-private
qtConfig(socketcan) {
    SUBDIRS += socketcan
}

SUBDIRS += peakcan tinycan
win32:SUBDIRS += vectorcan
