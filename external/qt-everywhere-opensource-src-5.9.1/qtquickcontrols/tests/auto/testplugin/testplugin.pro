TEMPLATE = lib
CONFIG += plugin
TARGET  = testplugin
TARGETPATH = QtQuickControlsTests

QT += qml quick core-private gui-private
!no_desktop: QT += widgets


QMLDIR = $$PWD/$$TARGETPATH/qmldir

OTHER_FILES += \
    $$PWD/testplugin.json \
    $$QMLDIR

SOURCES += \
    $$PWD/testplugin.cpp

HEADERS += \
    $$PWD/testplugin.h \
    $$PWD/testcppmodels.h \
    $$PWD/../shared/testmodel.h

mac {
    LIBS += -framework Carbon
}

DESTDIR = $$TARGETPATH

!equals(PWD, $$OUT_PWD) {
    # move qmldir alongside the plugin if shadow build
    qmldirfile.files = $$QMLDIR
    qmldirfile.path = $$DESTDIR
    COPIES += qmldirfile
}
