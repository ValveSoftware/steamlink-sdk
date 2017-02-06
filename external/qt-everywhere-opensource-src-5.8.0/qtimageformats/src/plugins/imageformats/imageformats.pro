TEMPLATE = subdirs
SUBDIRS = \
#    dds \
    icns \
    tga \
    tiff \
    wbmp \
    webp

config_libmng: SUBDIRS += mng
config_jasper {
    SUBDIRS += jp2
} else:darwin: {
    SUBDIRS += macjp2
}

winrt {
    SUBDIRS -= tiff \
               tga \
               webp
}
