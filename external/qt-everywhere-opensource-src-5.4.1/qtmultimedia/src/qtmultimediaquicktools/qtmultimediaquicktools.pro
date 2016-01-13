TARGET = QtMultimediaQuick_p
QT = core quick multimedia-private
CONFIG += internal_module

load(qt_module)

DEFINES += QT_BUILD_QTMM_QUICK_LIB

# Header files must go inside source directory of a module
# to be installed by syncqt.
INCLUDEPATH += ../multimedia/qtmultimediaquicktools_headers/

PRIVATE_HEADERS += \
    ../multimedia/qtmultimediaquicktools_headers/qdeclarativevideooutput_p.h \
    ../multimedia/qtmultimediaquicktools_headers/qdeclarativevideooutput_backend_p.h \
    ../multimedia/qtmultimediaquicktools_headers/qsgvideonode_p.h \
    ../multimedia/qtmultimediaquicktools_headers/qtmultimediaquickdefs_p.h

SOURCES += \
    qsgvideonode_p.cpp \
    qdeclarativevideooutput.cpp \
    qdeclarativevideooutput_render.cpp \
    qdeclarativevideooutput_window.cpp \
    qsgvideonode_i420.cpp \
    qsgvideonode_rgb.cpp \
    qsgvideonode_texture.cpp

HEADERS += \
    $$PRIVATE_HEADERS \
    qdeclarativevideooutput_render_p.h \
    qdeclarativevideooutput_window_p.h \
    qsgvideonode_i420.h \
    qsgvideonode_rgb.h \
    qsgvideonode_texture.h
