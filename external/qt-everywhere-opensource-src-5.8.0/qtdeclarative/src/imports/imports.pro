TEMPLATE = subdirs

SUBDIRS += \
    builtins \
    qtqml \
    folderlistmodel \
    localstorage \
    models \
    statemachine

qtConfig(settings): SUBDIRS += settings

qtHaveModule(quick) {
    SUBDIRS += \
        layouts \
        qtquick2 \
        window \
        testlib

    qtConfig(opengl(es1|es2)?): \
        SUBDIRS += particles
}

qtHaveModule(xmlpatterns) : SUBDIRS += xmllistmodel
