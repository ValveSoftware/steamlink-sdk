TEMPLATE = lib
TARGET = pngdumper
QT += core quick
CONFIG += plugin

TARGET = $$qtLibraryTarget($$TARGET)
uri = org.qtproject.pngdumper

# Input
SOURCES += \
    pngdumper_plugin.cpp \
    pngdumper.cpp

HEADERS += \
    pngdumper_plugin.h \
    pngdumper.h

OTHER_FILES = qmldir \
    pngdumper.qml \
    ItemModel.qml

!equals(_PRO_FILE_PWD_, $$OUT_PWD) {
    cpqmldir.files = qmldir
    cpqmldir.path = $$OUT_PWD
    COPIES += cpqmldir
}

qmldir.files = qmldir
unix {
    maemo5 | !isEmpty(MEEGO_VERSION_MAJOR) {
        installPath = /usr/lib/qt4/imports/$$replace(uri, \\., /)
    } else {
        installPath = $$[QT_INSTALL_IMPORTS]/$$replace(uri, \\., /)
    }
    qmldir.path = $$installPath
    target.path = $$installPath
    INSTALLS += target qmldir
}



