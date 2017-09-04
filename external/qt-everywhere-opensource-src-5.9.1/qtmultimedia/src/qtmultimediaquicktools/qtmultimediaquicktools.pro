TARGET = QtMultimediaQuick_p

QT = core quick multimedia-private
CONFIG += internal_module

# Header files must go inside source directory of a module
# to be installed by syncqt.
INCLUDEPATH += ../multimedia/qtmultimediaquicktools_headers/
VPATH += ../multimedia/qtmultimediaquicktools_headers/

PRIVATE_HEADERS += \
    qdeclarativevideooutput_p.h \
    qdeclarativevideooutput_backend_p.h \
    qsgvideonode_p.h \
    qtmultimediaquickdefs_p.h

HEADERS += \
    $$PRIVATE_HEADERS \
    qdeclarativevideooutput_render_p.h \
    qdeclarativevideooutput_window_p.h \
    qsgvideonode_yuv_p.h \
    qsgvideonode_rgb_p.h \
    qsgvideonode_texture_p.h

SOURCES += \
    qsgvideonode_p.cpp \
    qdeclarativevideooutput.cpp \
    qdeclarativevideooutput_render.cpp \
    qdeclarativevideooutput_window.cpp \
    qsgvideonode_yuv.cpp \
    qsgvideonode_rgb.cpp \
    qsgvideonode_texture.cpp

RESOURCES += \
    qtmultimediaquicktools.qrc

OTHER_FILES += \
    shaders/monoplanarvideo.vert \
    shaders/rgbvideo_padded.vert \
    shaders/rgbvideo.frag \
    shaders/rgbvideo_swizzle.frag \
    shaders/biplanaryuvvideo.vert \
    shaders/biplanaryuvvideo.frag \
    shaders/biplanaryuvvideo_swizzle.frag \
    shaders/triplanaryuvvideo.vert \
    shaders/triplanaryuvvideo.frag \
    shaders/uyvyvideo.frag \
    shaders/yuyvvideo.frag

load(qt_module)
