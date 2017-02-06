TEMPLATE = subdirs
SUBDIRS = customclass

qtHaveModule(gui): SUBDIRS += qsdbg

qtHaveModule(widgets) {
    SUBDIRS += \
        helloscript \
        context2d \
        defaultprototypes

    qtHaveModule(uitools) {
        SUBDIRS += \
            calculator \
            qstetrix
    }

    !wince {
        SUBDIRS += \
            qscript
    }
}

!wince {
    qtHaveModule(gui): SUBDIRS += marshal
}

maemo5: CONFIG += qt_example
