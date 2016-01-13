TEMPLATE = lib

TARGET = qgsttools_p
QPRO_PWD = $$PWD
QT = core-private multimedia-private gui-private

!static:DEFINES += QT_MAKEDLL
DEFINES += GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_26

unix:!maemo*:contains(QT_CONFIG, alsa) {
DEFINES += HAVE_ALSA
LIBS_PRIVATE += \
    -lasound
}

CONFIG += link_pkgconfig

PKGCONFIG_PRIVATE += \
    gstreamer-0.10 \
    gstreamer-base-0.10 \
    gstreamer-interfaces-0.10 \
    gstreamer-audio-0.10 \
    gstreamer-video-0.10 \
    gstreamer-pbutils-0.10

maemo*: PKGCONFIG_PRIVATE +=gstreamer-plugins-bad-0.10

config_resourcepolicy {
    DEFINES += HAVE_RESOURCE_POLICY
    PKGCONFIG_PRIVATE += libresourceqt5
}

# Header files must go inside source directory of a module
# to be installed by syncqt.
INCLUDEPATH += ../multimedia/gsttools_headers/
VPATH += ../multimedia/gsttools_headers/

PRIVATE_HEADERS += \
    qgstbufferpoolinterface_p.h \
    qgstreamerbushelper_p.h \
    qgstreamermessage_p.h \
    qgstutils_p.h \
    qgstvideobuffer_p.h \
    qvideosurfacegstsink_p.h \
    qgstreamervideorendererinterface_p.h \
    qgstreameraudioinputselector_p.h \
    qgstreamervideorenderer_p.h \
    qgstreamervideoinputdevicecontrol_p.h \
    gstvideoconnector_p.h \
    qgstcodecsinfo_p.h \
    qgstreamervideoprobecontrol_p.h \
    qgstreameraudioprobecontrol_p.h \
    qgstreamervideowindow_p.h

SOURCES += \
    qgstbufferpoolinterface.cpp \
    qgstreamerbushelper.cpp \
    qgstreamermessage.cpp \
    qgstutils.cpp \
    qgstvideobuffer.cpp \
    qvideosurfacegstsink.cpp \
    qgstreamervideorendererinterface.cpp \
    qgstreameraudioinputselector.cpp \
    qgstreamervideorenderer.cpp \
    qgstreamervideoinputdevicecontrol.cpp \
    qgstcodecsinfo.cpp \
    gstvideoconnector.c \
    qgstreamervideoprobecontrol.cpp \
    qgstreameraudioprobecontrol.cpp \
    qgstreamervideowindow.cpp

qtHaveModule(widgets) {
    QT += multimediawidgets

    PRIVATE_HEADERS += \
        qgstreamervideowidget_p.h

    SOURCES += \
        qgstreamervideowidget.cpp
}

maemo6 {
    PKGCONFIG_PRIVATE += qmsystem2

    contains(QT_CONFIG, opengles2):qtHaveModule(widgets) {
        PRIVATE_HEADERS += qgstreamergltexturerenderer_p.h
        SOURCES += qgstreamergltexturerenderer.cpp
        QT += opengl
        LIBS_PRIVATE += -lEGL -lgstmeegointerfaces-0.10
    }
}

config_gstreamer_appsrc {
    PKGCONFIG_PRIVATE += gstreamer-app-0.10
    PRIVATE_HEADERS += qgstappsrc_p.h
    SOURCES += qgstappsrc.cpp

    DEFINES += HAVE_GST_APPSRC

    LIBS_PRIVATE += -lgstapp-0.10
}

config_linux_v4l: DEFINES += USE_V4L

HEADERS += $$PRIVATE_HEADERS

DESTDIR = $$QT.multimedia.libs
target.path = $$[QT_INSTALL_LIBS]

INSTALLS += target
