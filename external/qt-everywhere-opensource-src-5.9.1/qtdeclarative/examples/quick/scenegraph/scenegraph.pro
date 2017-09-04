TEMPLATE = subdirs

qtConfig(opengl(es1|es2)?) {
    SUBDIRS += \
            graph \
            simplematerial \
            sgengine \
            textureinsgnode \
            openglunderqml \
            textureinthread \
            twotextureproviders
}

SUBDIRS += \
        customgeometry \
        rendernode \
        threadedanimation

EXAMPLE_FILES += \
    shared
