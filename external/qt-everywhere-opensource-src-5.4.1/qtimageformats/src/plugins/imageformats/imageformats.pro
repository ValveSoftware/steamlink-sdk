TEMPLATE = subdirs
SUBDIRS = \
    dds \
    icns \
    jp2 \
    mng \
    tga \
    tiff \
    wbmp \
    webp

wince:SUBDIRS -= jp2

winrt {
    SUBDIRS -= tiff \
               tga
}

winrt|android|ios: SUBDIRS -= webp
