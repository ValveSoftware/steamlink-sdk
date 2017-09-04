TEMPLATE = lib

CONFIG -= qt
CONFIG += exceptions
CONFIG += warn_off

contains(QT_CONFIG, debug_and_release): CONFIG += debug_and_release
contains(QT_CONFIG, build_all): CONFIG += build_all

INCLUDEPATH += $$PWD/include

load(qt_build_paths)

!isEmpty(LIPILIBS) {
    LIBS += -L$$MODULE_BASE_OUTDIR/lib
    for (lib, LIPILIBS) {
        LIBS += -l$$lib$$qtPlatformTargetSuffix()
    }
}

TARGET = $$TARGET$$qtPlatformTargetSuffix()
