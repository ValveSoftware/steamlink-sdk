TEMPLATE = lib

CONFIG -= qt
CONFIG += exceptions
CONFIG += warn_off

INCLUDEPATH += $$PWD/include

load(qt_build_paths)

!isEmpty(LIPILIBS) {
    LIBS += -L$$MODULE_BASE_OUTDIR/lib
    for (lib, LIPILIBS) {
        LIBS += -l$$lib$$qtPlatformTargetSuffix()
    }
}

TARGET = $$TARGET$$qtPlatformTargetSuffix()
