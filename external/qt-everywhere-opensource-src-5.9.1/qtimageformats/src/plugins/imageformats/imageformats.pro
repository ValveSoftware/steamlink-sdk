TEMPLATE = subdirs
SUBDIRS = \
#    dds \
    tga \
    tiff \
    wbmp \
    webp

qtConfig(regularexpression): \
    SUBDIRS += icns

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
