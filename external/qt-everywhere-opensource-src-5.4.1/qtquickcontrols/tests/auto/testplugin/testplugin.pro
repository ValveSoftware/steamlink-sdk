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
    $$PWD/testcppmodels.h

mac {
    LIBS += -framework Carbon
}

DESTDIR = $$TARGETPATH

!equals(PWD, $$OUT_PWD) {
    # move qmldir alongside the plugin if shadow build
    qmldirfile.input = QMLDIR
    qmldirfile.output = $$DESTDIR/qmldir
    qmldirfile.variable_out = PRE_TARGETDEPS
    qmldirfile.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
    qmldirfile.CONFIG = no_link no_clean
    QMAKE_EXTRA_COMPILERS += qmldirfile
}
